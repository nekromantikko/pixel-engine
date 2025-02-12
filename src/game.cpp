#include "game.h"
#include "system.h"
#include "input.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "rendering_util.h"
#include "level.h"
#include "viewport.h"
#include "collision.h"
#include "metasprite.h"
#include "tiles.h"
#include <imgui.h>
#include "math.h"
#include <vector>
#include "audio.h"

namespace Game {
    r64 secondsElapsed = 0.0f;

    // Seconds elapsed while not paused
    r64 gameplaySecondsElapsed = 0.0f;

    // Rendering data
    RenderSettings* pRenderSettings;
    Sprite* pSprites;
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    // Viewport
    Viewport viewport;

    Level* pCurrentLevel = nullptr;

    Pool<Actor> actors;
    PoolHandle<Actor> playerHandle;
    Pool<PoolHandle<Actor>> actorRemoveList;

    ChrSheet playerBank;

    bool paused = false;

    constexpr r32 damageDelay = 0.5f;

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;

    u8 basePaletteColors[PALETTE_MEMORY_SIZE];

    // TODO: Try to eliminate as much of this as possible
    constexpr s32 playerGrenadePrototypeIndex = 1;
    constexpr s32 playerArrowPrototypeIndex = 4;
    constexpr s32 dmgNumberPrototypeIndex = 5;
    constexpr s32 enemyFireballPrototypeIndex = 8;

    constexpr u8 playerWingFrameBankOffsets[4] = { 0x00, 0x08, 0x10, 0x18 };
    constexpr u8 playerHeadFrameBankOffsets[12] = { 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C };
    constexpr u8 playerLegsFrameBankOffsets[4] = { 0x50, 0x54, 0x58, 0x5C };
    constexpr u8 playerBowFrameBankOffsets[3] = { 0x60, 0x68, 0x70 };
    constexpr u8 playerLauncherFrameBankOffsets[3] = { 0x80, 0x88, 0x90 };

    constexpr u8 playerWingFrameChrOffset = 0x00;
    constexpr u8 playerHeadFrameChrOffset = 0x08;
    constexpr u8 playerLegsFrameChrOffset = 0x0C;
    constexpr u8 playerWeaponFrameChrOffset = 0x18;

    constexpr u8 playerWingFrameTileCount = 8;
    constexpr u8 playerHeadFrameTileCount = 4;
    constexpr u8 playerLegsFrameTileCount = 4;
    constexpr u8 playerHandFrameTileCount = 2;
    constexpr u8 playerWeaponFrameTileCount = 8;

    constexpr IVec2 playerBowOffsets[3] = { { 10, -4 }, { 9, -14 }, { 10, 4 } };
    constexpr u32 playerBowFwdMetaspriteIndex = 8;
    constexpr u32 playerBowDiagMetaspriteIndex = 9;
    constexpr u32 playerBowArrowFwdMetaspriteIndex = 3;
    constexpr u32 playerBowArrowDiagMetaspriteIndex = 4;

    constexpr IVec2 playerLauncherOffsets[3] = { { 5, -5 }, { 7, -12 }, { 8, 1 } };
    constexpr u32 playerLauncherFwdMetaspriteIndex = 10;
    constexpr u32 playerLauncherDiagMetaspriteIndex = 11;
    constexpr u32 playerLauncherGrenadeMetaspriteIndex = 12;

    constexpr Vec2 viewportScrollThreshold = { 4.0f, 3.0f };

#pragma region Viewport
    static void UpdateScreenScroll() {
        const Scanline state = {
            (s32)(viewport.x * METATILE_DIM_PIXELS),
            (s32)(viewport.y * METATILE_DIM_PIXELS)
        };
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = state;
        }
    }

    static void UpdateViewport() {
        Actor* pPlayer = actors.Get(playerHandle);
        if (pPlayer == nullptr) {
            return;
        }

        Vec2 viewportCenter = Vec2{ viewport.x + VIEWPORT_WIDTH_METATILES / 2.0f, viewport.y + VIEWPORT_HEIGHT_METATILES / 2.0f };
        Vec2 targetOffset = pPlayer->position - viewportCenter;

        Vec2 delta = { 0.0f, 0.0f };
        if (targetOffset.x > viewportScrollThreshold.x) {
            delta.x = targetOffset.x - viewportScrollThreshold.x;
        }
        else if (targetOffset.x < -viewportScrollThreshold.x) {
            delta.x = targetOffset.x + viewportScrollThreshold.x;
        }

        if (targetOffset.y > viewportScrollThreshold.y) {
            delta.y = targetOffset.y - viewportScrollThreshold.y;
        }
        else if (targetOffset.y < -viewportScrollThreshold.y) {
            delta.y = targetOffset.y + viewportScrollThreshold.y;
        }

        MoveViewport(&viewport, pNametables, &pCurrentLevel->tilemap, delta.x, delta.y);
    }

    static bool PositionInViewportBounds(Vec2 pos) {
        return pos.x >= viewport.x &&
            pos.x < viewport.x + VIEWPORT_WIDTH_METATILES &&
            pos.y >= viewport.y &&
            pos.y < viewport.y + VIEWPORT_HEIGHT_METATILES;
    }

    static IVec2 WorldPosToScreenPixels(Vec2 pos) {
        return IVec2{
            (s32)round((pos.x - viewport.x) * METATILE_DIM_PIXELS),
            (s32)round((pos.y - viewport.y) * METATILE_DIM_PIXELS)
        };
    }
#pragma endregion

#pragma region Actor initialization
    static void InitializeActor(Actor* pActor) {
        pActor->flags.facingDir = ACTOR_FACING_RIGHT;
        pActor->flags.inAir = true;
        pActor->flags.active = true;
        pActor->flags.pendingRemoval = false;

        pActor->initialPosition = pActor->position;

        pActor->damageCounter = 0;
        pActor->lifetimeCounter = pActor->lifetime;
    }

    static Actor* SpawnActor(const Actor* pTemplate) {
        auto handle = actors.Add(*pTemplate);

        if (handle == PoolHandle<Actor>::Null()) {
            return nullptr;
        }

        if (pTemplate->pPrototype->behaviour == ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER) {
            playerHandle = handle;
        }

        return actors.Get(handle);
    }

    static Actor* SpawnActor(u32 presetIndex) {
        auto handle = actors.Add();

        if (handle == PoolHandle<Actor>::Null()) {
            return nullptr;
        }

        Actor* pActor = actors.Get(handle);

        const ActorPrototype* pPrototype = Actors::GetPrototype(presetIndex);
        pActor->pPrototype = pPrototype;
        if (pPrototype->behaviour == ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER) {
            playerHandle = handle;
        }

        return pActor;
    }
#pragma endregion

#pragma region Actor utils
    // Returns false if counter stops, true if keeps running
    static bool UpdateCounter(r32& counter, r64 dt) {
        if (counter > dt) {
            counter -= dt;
            return true;
        }
        else {
            counter = 0.0f;
            return false;
        }
    }
#pragma endregion

#pragma region Rendering
    static void DrawActor(const Actor* pActor, Sprite** ppNextSprite, s32 frameIndex = 0, const IVec2& pixelOffset = {0,0}, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1) {
        // Culling
        if (!PositionInViewportBounds(pActor->position)) {
            return;
        }

        IVec2 drawPos = WorldPosToScreenPixels(pActor->position) + pixelOffset;
        const ActorAnimFrame& frame = pActor->pPrototype->frames[frameIndex];

        switch (pActor->pPrototype->animMode) {
        case ACTOR_ANIM_MODE_SPRITES: {
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(frame.metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos + frame.spriteIndex, *ppNextSprite, 1, drawPos, hFlip, vFlip, paletteOverride);
            (*ppNextSprite)++;
            break;
        }
        case ACTOR_ANIM_MODE_METASPRITES: {
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(frame.metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos, *ppNextSprite, pMetasprite->spriteCount, drawPos, hFlip, vFlip, paletteOverride);
            *ppNextSprite += pMetasprite->spriteCount;
            break;
        }
        default:
            break;
        }
    }

    static s32 GetDamagePaletteOverride(Actor* pActor) {
        return (pActor->damageCounter > 0) ? (s32)(gameplaySecondsElapsed * 20) % 4 : -1;
    }

    static s32 GetAnimFrameFromDirection(const Vec2& dir, u32 frameCount) {
        const r32 angle = atan2f(dir.y, dir.x);
        return (s32)roundf(((angle + pi) / (pi * 2)) * frameCount) % frameCount;
    }

    static s32 GetAnimFrameFromLifetime(r32 lifetime, r32 lifetimeMax, u32 frameCount) {
        return std::min((u32)roundf((lifetime / lifetimeMax) * frameCount), frameCount - 1);
    }
#pragma endregion

#pragma region Collision
    static bool ActorsColliding(const Actor* pActor, const Actor* pOther) {
        const AABB& hitbox = pActor->pPrototype->hitbox;
        const AABB& hitboxOther = pOther->pPrototype->hitbox;
        if (Collision::BoxesOverlap(hitbox, pActor->position, hitboxOther, pOther->position)) {
            return true;
        }

        return false;
    }

    // TODO: Actor collisions could use HitResult as well...
    static void ForEachActorCollision(Actor* pActor, u32 layer, void (*callback)(Actor*, Actor*)) {
        for (u32 i = 0; i < actors.Count(); i++)
        {
            if (!pActor->flags.active || pActor->flags.pendingRemoval) {
                break;
            }

            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pOther = actors.Get(handle);

            if (pOther == nullptr || pOther->pPrototype->collisionLayer != layer || pOther->flags.pendingRemoval || !pOther->flags.active) {
                continue;
            }

            const AABB& hitbox = pActor->pPrototype->hitbox;
            const AABB& hitboxOther = pOther->pPrototype->hitbox;
            if (ActorsColliding(pActor, pOther)) {
                callback(pActor, pOther);
            }
        }
    }

    static bool ActorCollidesWithPlayer(Actor* pActor, Actor** outPlayer) {
        Actor* pPlayer = actors.Get(playerHandle);
        if (pPlayer != nullptr) {
            if (ActorsColliding(pActor, pPlayer)) {
                *outPlayer = pPlayer;
                return true;
            }
        }
        return false;
    }
#pragma endregion

#pragma region Movement
    static void ActorFacePlayer(Actor* pActor) {
        pActor->flags.facingDir = ACTOR_FACING_RIGHT;

        Actor* pPlayer = actors.Get(playerHandle);
        if (pPlayer == nullptr) {
            return;
        }

        if (pPlayer->position.x < pActor->position.x) {
            pActor->flags.facingDir = ACTOR_FACING_LEFT;
        }
    }

    static bool ActorMoveHorizontal(Actor* pActor, r64 dt, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dx = pActor->velocity.x * dt;

        Collision::SweepBoxHorizontal(&pCurrentLevel->tilemap, hitbox, pActor->position, dx, outHit);
        pActor->position.x = outHit.location.x;
        return outHit.blockingHit;
    }

    static bool ActorMoveVertical(Actor* pActor, r64 dt, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dy = pActor->velocity.y * dt;

        Collision::SweepBoxVertical(&pCurrentLevel->tilemap, hitbox, pActor->position, dy, outHit);
        pActor->position.y = outHit.location.y;
        return outHit.blockingHit;
    }

    static void ApplyGravity(Actor* pActor, r32 gravity, r64 dt) {
        pActor->velocity.y += gravity * dt;
    }
#pragma endregion

#pragma region Damage
    static void SpawnExplosion(const Vec2& position, u32 prototypeIndex) {
        // TODO: Make the prototype a struct member
        Actor* pHit = SpawnActor(prototypeIndex);
        if (pHit == nullptr) {
            return;
        }

        pHit->position = position;
        pHit->lifetime = 0.16f;
        pHit->velocity = Vec2{};
        InitializeActor(pHit);
    }

    static void ActorDie(Actor* pActor, const Vec2& explosionPos) {
        pActor->flags.pendingRemoval = true;
        SpawnExplosion(explosionPos, pActor->pPrototype->deathEffect);
    }

    static bool ActorTakeDamage(Actor* pActor, u32 damage) {
        pActor->health -= damage;
        pActor->damageCounter = damageDelay;

        // Spawn damage numbers
        Actor* pDmg = SpawnActor(dmgNumberPrototypeIndex);
        if (pDmg != nullptr) {
            pDmg->drawNumber = damage;

            const AABB& hitbox = pActor->pPrototype->hitbox;
            // Random point inside hitbox
            const Vec2 randomPointInsideHitbox = {
                ((r32)rand() / RAND_MAX) * (hitbox.x2 - hitbox.x1) + hitbox.x1,
                ((r32)rand() / RAND_MAX) * (hitbox.y2 - hitbox.y1) + hitbox.y1
            };
            pDmg->position = pActor->position + randomPointInsideHitbox;

            pDmg->lifetime = 1.0f;
            pDmg->velocity = { 0, -1.5f };

            InitializeActor(pDmg);
        }

        if (pActor->health <= 0) {
            return false;
        }

        return true;
    }
#pragma endregion

#pragma region Player logic
    static void HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy) {
        // If invulnerable
        if (pPlayer->damageCounter != 0) {
            return;
        }

        const u32 damage = (rand() % 2) + 1;
        if (!ActorTakeDamage(pPlayer, damage)) {
            // TODO: Player death
        }

        // Recoil
        if (pEnemy->position.x > pPlayer->position.x) {
            pPlayer->flags.facingDir = 1;
            pPlayer->velocity.x = -5;
        }
        else {
            pPlayer->flags.facingDir = -1;
            pPlayer->velocity.x = 5;
        }

    }

    static void PlayerInput(Actor* pPlayer, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;
        if (Input::ButtonDown(BUTTON_DPAD_LEFT)) {
            pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
            pPlayer->velocity.x = -6.25f;
        }
        else if (Input::ButtonDown(BUTTON_DPAD_RIGHT)) {
            pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
            pPlayer->velocity.x = 6.25f;
        }
        else {
            pPlayer->velocity.x = 0;
        }

        // Aim mode
        if (Input::ButtonDown(BUTTON_DPAD_UP)) {
            playerState.aimMode = PLAYER_AIM_UP;
        }
        else if (Input::ButtonDown(BUTTON_DPAD_DOWN)) {
            playerState.aimMode = PLAYER_AIM_DOWN;
        }
        else playerState.aimMode = PLAYER_AIM_FWD;

        if (Input::ButtonPressed(BUTTON_START)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if (Input::ButtonPressed(BUTTON_A) && (!pPlayer->flags.inAir || !playerState.doubleJumped)) {
            pPlayer->velocity.y = -15.625f;
            if (pPlayer->flags.inAir) {
                playerState.doubleJumped = true;
            }

            // Trigger new flap
            playerState.wingFrame++;

            Audio::PlaySFX(&jumpSfx, CHAN_ID_PULSE0);
        }

        if (pPlayer->velocity.y < 0 && Input::ButtonReleased(BUTTON_A)) {
            pPlayer->velocity.y /= 2;
        }

        if (Input::ButtonDown(BUTTON_A) && pPlayer->velocity.y > 0) {
            playerState.slowFall = true;
        }

        if (Input::ButtonReleased(BUTTON_B)) {
            playerState.shootCounter = 0.0f;
        }

        if (Input::ButtonPressed(BUTTON_SELECT)) {
            if (playerState.weapon == PLAYER_WEAPON_LAUNCHER) {
                playerState.weapon = PLAYER_WEAPON_BOW;
            }
            else playerState.weapon = PLAYER_WEAPON_LAUNCHER;
        }
    }

    static void PlayerShoot(Actor* pPlayer, r64 dt) {
        constexpr r32 shootDelay = 0.16f;

        PlayerState& playerState = pPlayer->playerState;
        if (playerState.shootCounter > dt) {
            playerState.shootCounter -= dt;
        }
        else playerState.shootCounter = 0.0f;

        if (Input::ButtonDown(BUTTON_B) && playerState.shootCounter <= 0.0f) {
            playerState.shootCounter += shootDelay;

            const s32 prototypeIndex = playerState.weapon == PLAYER_WEAPON_LAUNCHER ? playerGrenadePrototypeIndex : playerArrowPrototypeIndex;
            Actor* pBullet = SpawnActor(prototypeIndex);
            if (pBullet == nullptr) {
                return;
            }

            const Vec2 fwdOffset = Vec2{ 0.375f * pPlayer->flags.facingDir, -0.25f };
            const Vec2 upOffset = Vec2{ 0.1875f * pPlayer->flags.facingDir, -0.5f };
            const Vec2 downOffset = Vec2{ 0.25f * pPlayer->flags.facingDir, -0.125f };

            pBullet->position = pPlayer->position;
            pBullet->velocity = Vec2{};
            pBullet->gravity = 140;
            pBullet->lifetime = 3.0f;

            if (playerState.aimMode == PLAYER_AIM_FWD) {
                pBullet->position = pBullet->position + fwdOffset;
                pBullet->velocity.x = 40.0f * pPlayer->flags.facingDir;
            }
            else {
                pBullet->velocity.x = 28.28f * pPlayer->flags.facingDir;
                pBullet->velocity.y = (playerState.aimMode == PLAYER_AIM_UP) ? -28.28f : 28.28f;
                pBullet->position = pBullet->position + ((playerState.aimMode == PLAYER_AIM_UP) ? upOffset : downOffset);
            }

            if (playerState.weapon == PLAYER_WEAPON_LAUNCHER) {
            pBullet->velocity = pBullet->velocity * 0.75f;
            Audio::PlaySFX(&gunSfx, CHAN_ID_NOISE);
            }
            pBullet->velocity = pBullet->velocity + pPlayer->velocity * dt;

            InitializeActor(pBullet);
        }
    }

    static void DrawPlayerGun(Actor* pPlayer, r32 vOffset, Sprite** ppNextSprite) {
        IVec2 drawPos = WorldPosToScreenPixels(pPlayer->position);
        drawPos.y += vOffset;

        // Draw weapon first
        IVec2 weaponOffset;
        u8 weaponFrameBankOffset;
        u32 weaponMetaspriteIndex;
        switch (pPlayer->playerState.weapon) {
        case PLAYER_WEAPON_BOW: {
            weaponOffset = playerBowOffsets[pPlayer->playerState.aimMode];
            weaponFrameBankOffset = playerBowFrameBankOffsets[pPlayer->playerState.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.aimMode == PLAYER_AIM_FWD ? playerBowFwdMetaspriteIndex : playerBowDiagMetaspriteIndex;
            break;
        }
        case PLAYER_WEAPON_LAUNCHER: {
            weaponOffset = playerLauncherOffsets[pPlayer->playerState.aimMode];
            weaponFrameBankOffset = playerLauncherFrameBankOffsets[pPlayer->playerState.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.aimMode == PLAYER_AIM_FWD ? playerLauncherFwdMetaspriteIndex : playerLauncherDiagMetaspriteIndex;
            break;
        }
        default:
            break;
        }
        weaponOffset.x *= pPlayer->flags.facingDir;

        Rendering::Util::CopyChrTiles(playerBank.tiles + weaponFrameBankOffset, pChr[1].tiles + playerWeaponFrameChrOffset, playerWeaponFrameTileCount);

        const Metasprite* bowMetasprite = Metasprites::GetMetasprite(weaponMetaspriteIndex);
        Rendering::Util::CopyMetasprite(bowMetasprite->spritesRelativePos, *ppNextSprite, bowMetasprite->spriteCount, drawPos + weaponOffset, pPlayer->flags.facingDir == ACTOR_FACING_LEFT, false);
        *ppNextSprite += bowMetasprite->spriteCount;
    }

    static void DrawPlayer(Actor* pPlayer, Sprite** ppNextSprite, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;

        // Animate chr sheet using player bank
        const bool jumping = pPlayer->velocity.y < 0;
        const bool descending = !jumping && pPlayer->velocity.y > 0;
        const bool falling = descending && !playerState.slowFall;
        const bool moving = abs(pPlayer->velocity.x) > 0;
        const bool takingDamage = pPlayer->damageCounter > 0;

        s32 headFrameIndex = PLAYER_HEAD_IDLE;
        if (takingDamage) {
            headFrameIndex = PLAYER_HEAD_DMG;
        }
        else if (falling) {
            headFrameIndex = PLAYER_HEAD_FALL;
        }
        else if (moving) {
            headFrameIndex = PLAYER_HEAD_FWD;
        }
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerHeadFrameBankOffsets[playerState.aimMode * 4 + headFrameIndex],
            pChr[1].tiles + playerHeadFrameChrOffset,
            playerHeadFrameTileCount
        );

        s32 legsFrameIndex = PLAYER_LEGS_IDLE;
        if (descending || takingDamage) {
            legsFrameIndex = PLAYER_LEGS_FALL;
        }
        else if (jumping) {
            legsFrameIndex = PLAYER_LEGS_JUMP;
        }
        else if (moving) {
            legsFrameIndex = PLAYER_LEGS_FWD;
        }
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerLegsFrameBankOffsets[legsFrameIndex],
            pChr[1].tiles + playerLegsFrameChrOffset,
            playerLegsFrameTileCount
        );

        // When jumping or falling, wings get into proper position and stay there for the duration of the jump/fall
        const bool wingsInPosition = (jumping && playerState.wingFrame == PLAYER_WINGS_ASCEND) || (falling && playerState.wingFrame == PLAYER_WINGS_DESCEND);

        // Wings flap faster to get into proper position
        const r32 wingAnimFrameLength = (jumping || falling) ? 0.09f : 0.18f;
        playerState.wingCounter += dt / wingAnimFrameLength;
        while (playerState.wingCounter > 1.0f) {
            // If wings not in position, keep flapping
            if (!wingsInPosition) {
                playerState.wingFrame++;
            }
            playerState.wingCounter -= 1.0f;
        }
        playerState.wingFrame %= 4;

        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerWingFrameBankOffsets[playerState.wingFrame],
            pChr[1].tiles + playerWingFrameChrOffset,
            playerWingFrameTileCount
        );

        // Setup draw data
        s32 vOffset = 0;
        if (pPlayer->velocity.y == 0) {
            vOffset = playerState.wingFrame > PLAYER_WINGS_FLAP_START ? -1 : 0;
        }

        DrawPlayerGun(pPlayer, vOffset, ppNextSprite);
        const s32 paletteOverride = GetDamagePaletteOverride(pPlayer);
        DrawActor(pPlayer, ppNextSprite, playerState.aimMode, { 0, vOffset }, pPlayer->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }

    static void UpdatePlayer(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        UpdateCounter(pActor->damageCounter, dt);
        
        // Reset slow fall
        pActor->playerState.slowFall = false;

        const bool playerStunned = pActor->damageCounter > 0;
        if (!playerStunned) {
            PlayerInput(pActor, dt);
            PlayerShoot(pActor, dt);
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, dt, hit)) {
            pActor->velocity.x = 0.0f;
        }

        const r32 gravity = pActor->playerState.slowFall ? pActor->gravity / 4 : pActor->gravity;
        ApplyGravity(pActor, gravity, dt);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, dt, hit)) {
            pActor->velocity.y = 0.0f;

            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
                pActor->playerState.doubleJumped = false;
            }
        }

        // TODO: collision with other actors

        DrawPlayer(pActor, ppNextSprite, dt);
    }
#pragma endregion

#pragma Bullets
    static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
        ActorDie(pBullet, pBullet->position);

        const u32 damage = (rand() % 2) + 1;
        if (!ActorTakeDamage(pEnemy, damage)) {
            ActorDie(pEnemy, pEnemy->position);
        }
    }

    static void UpdateBullet(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        if (!UpdateCounter(pActor->lifetimeCounter, dt)) {
            ActorDie(pActor, pActor->position);
            return;
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, dt, hit)) {
            ActorDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, dt, hit)) {
            ActorDie(pActor, hit.impactPoint);
            return;
        }

        ForEachActorCollision(pActor, ACTOR_COLLISION_LAYER_ENEMY, HandleBulletEnemyCollision);

        const u32 frameCount = pActor->pPrototype->frameCount;
        const s32 frameIndex = GetAnimFrameFromDirection(pActor->velocity.Normalize(), frameCount);

        DrawActor(pActor, ppNextSprite, frameIndex);
    }

    static void BulletRicochet(Vec2& velocity, const Vec2& normal) {
        velocity = velocity - 2 * DotProduct(velocity, normal) * normal;
        Audio::PlaySFX(&ricochetSfx, CHAN_ID_PULSE1);
    }

    static void UpdateBouncyBullet(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        if (!UpdateCounter(pActor->lifetimeCounter, dt)) {
            ActorDie(pActor, pActor->position);
        }

        ApplyGravity(pActor, pActor->gravity, dt);

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, dt, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        if (ActorMoveVertical(pActor, dt, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        ForEachActorCollision(pActor, ACTOR_COLLISION_LAYER_ENEMY, HandleBulletEnemyCollision);

        const u32 frameCount = pActor->pPrototype->frameCount;
        const s32 frameIndex = GetAnimFrameFromDirection(pActor->velocity.Normalize(), frameCount);

        DrawActor(pActor, ppNextSprite, frameIndex);
    }
#pragma endregion

#pragma region Enemy bullets
    static void UpdateEnemyFireball(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        if (!UpdateCounter(pActor->lifetimeCounter, dt)) {
            ActorDie(pActor, pActor->position);
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, dt, hit)) {
            ActorDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, dt, hit)) {
            ActorDie(pActor, hit.impactPoint);
            return;
        }

        Actor* pPlayer = nullptr;
        if (ActorCollidesWithPlayer(pActor, &pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
            ActorDie(pActor, pActor->position);
        }

        const u32 frameCount = pActor->pPrototype->frameCount;
        const s32 frameIndex = (s32)(gameplaySecondsElapsed / (pActor->animLength / frameCount)) % frameCount;

        DrawActor(pActor, ppNextSprite, frameIndex);
    }
#pragma endregion

#pragma region Enemies
    static void UpdateSlimeEnemy(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        UpdateCounter(pActor->damageCounter, dt);

        if (!pActor->flags.inAir) {
            const bool shouldJump = (rand() % 128) == 0;
            if (shouldJump) {
                pActor->velocity.y = -15.625f;
                ActorFacePlayer(pActor);
                pActor->velocity.x = 10.0f * pActor->flags.facingDir;
            }
            else {
                pActor->velocity.x = 0.5f * pActor->flags.facingDir;
            }
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, dt, hit)) {
            pActor->velocity.x = 0.0f;
            pActor->flags.facingDir = (s8)hit.impactNormal.x;
        }

        ApplyGravity(pActor, pActor->gravity, dt);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, dt, hit)) {
            pActor->velocity.y = 0.0f;

            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
            }
        }

        Actor* pPlayer = nullptr;
        if (ActorCollidesWithPlayer(pActor, &pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        const s32 paletteOverride = GetDamagePaletteOverride(pActor);
        DrawActor(pActor, ppNextSprite, 0, {0,0}, pActor->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }

    static void UpdateSkullEnemy(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        UpdateCounter(pActor->damageCounter, dt);

        ActorFacePlayer(pActor);

        static const r32 amplitude = 4.0f;
        const r32 sineTime = sin(gameplaySecondsElapsed);
        pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

        // Shoot fireballs
        const bool shouldFire = (rand() % 128) == 0;
        if (shouldFire) {

            Actor* pPlayer = actors.Get(playerHandle);
            if (pPlayer != nullptr) {
                Actor* pBullet = SpawnActor(enemyFireballPrototypeIndex);
                if (pBullet == nullptr) {
                    return;
                }

                pBullet->position = pActor->position;
                pBullet->lifetime = 10.0f;
                const Vec2 playerDir = (pPlayer->position - pActor->position).Normalize();
                pBullet->velocity = playerDir * 4.0f;

                InitializeActor(pBullet);
            }
        }

        Actor* pPlayer = nullptr;
        if (ActorCollidesWithPlayer(pActor, &pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        const s32 paletteOverride = GetDamagePaletteOverride(pActor);
        DrawActor(pActor, ppNextSprite, 0, { 0,0 }, pActor->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }
#pragma endregion

#pragma region Effects
    static void UpdateEffect(Actor* pActor, r64 dt) {
        if (!UpdateCounter(pActor->lifetimeCounter, dt)) {
            pActor->flags.pendingRemoval = true;
        }
    }

    static void UpdateExplosion(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        UpdateEffect(pActor, dt);

        const u32 frameCount = pActor->pPrototype->frameCount;
        const s32 frameIndex = GetAnimFrameFromLifetime(pActor->lifetimeCounter, pActor->lifetime, frameCount);

        DrawActor(pActor, ppNextSprite, frameIndex);
    }

    static void UpdateNumbers(Actor* pActor, Sprite** ppNextSprite, r64 dt) {
        UpdateEffect(pActor, dt);

        pActor->position.y += pActor->velocity.y * dt;

        static char numberStr[16]{};

        _itoa_s(pActor->drawNumber, numberStr, 10);
        const u32 strLength = strlen(numberStr);

        // Ascii character '0' = 0x30
        constexpr u8 chrOffset = 0x30;

        const u32 frameCount = pActor->pPrototype->frameCount;
        for (u32 c = 0; c < strLength; c++) {
            // TODO: What about metasprite frames? Handle this more rigorously!
            const s32 frameIndex = (numberStr[c] - chrOffset) % frameCount;
            const ActorAnimFrame& frame = pActor->pPrototype->frames[frameIndex];
            const u8 tileId = Metasprites::GetMetasprite(frame.metaspriteIndex)->spritesRelativePos[frame.spriteIndex].tileId;

            const IVec2 pixelPos = WorldPosToScreenPixels(pActor->position);
            const Sprite sprite = {
                pixelPos.y,
                pixelPos.x + c * 5,
                tileId,
                1
            };
            *((*ppNextSprite)++) = sprite;
        }
    }
#pragma endregion

    static void UpdateActors(Sprite** ppNextSprite, r64 dt) {
        
        actorRemoveList.Clear();

        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pActor = actors.Get(handle);

            if (pActor == nullptr) {
                continue;
            }

            if (pActor->flags.pendingRemoval) {
                actorRemoveList.Add(handle);
                continue;
            }

            if (!pActor->flags.active) {
                continue;
            }

            switch (pActor->pPrototype->behaviour) {
            case ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER: {
                UpdatePlayer(pActor, ppNextSprite, dt);
                break;
            }
            // Player projectiles
            case ACTOR_BEHAVIOUR_BULLET: {
                UpdateBullet(pActor, ppNextSprite, dt);
                break;
            }
            case ACTOR_BEHAVIOUR_BULLET_BOUNCY: {
                UpdateBouncyBullet(pActor, ppNextSprite, dt);
                break;
            }
            // Enemy projectiles
            case ACTOR_BEHAVIOUR_FIREBALL: {
                UpdateEnemyFireball(pActor, ppNextSprite, dt);
                break;
            }
            // Enemies
            case ACTOR_BEHAVIOUR_ENEMY_SLIME: {
                UpdateSlimeEnemy(pActor, ppNextSprite, dt);
                break;
            }
            case ACTOR_BEHAVIOUR_ENEMY_SKULL: {
                UpdateSkullEnemy(pActor, ppNextSprite, dt);
                break;
            }
            // Effects
            case ACTOR_BEHAVIOUR_FX_EXPLOSION: {
                UpdateExplosion(pActor, ppNextSprite, dt);
                break;
            }
            case ACTOR_BEHAVIOUR_FX_NUMBERS: {
                UpdateNumbers(pActor, ppNextSprite, dt);
                break;
            }
            default:
                break;
            }

        }

        for (u32 i = 0; i < actorRemoveList.Count(); i++) {
            auto handle = *actorRemoveList.Get(actorRemoveList.GetHandle(i));
            actors.Remove(handle);
        }
    }

#pragma region Public API
    void LoadLevel(u32 index, s32 screenIndex, bool refresh) {
        if (index >= MAX_LEVEL_COUNT) {
            DEBUG_ERROR("Level count exceeded!");
        }

        pCurrentLevel = Levels::GetLevelsPtr() + index;

        viewport.x = 0;
        viewport.y = 0;
        UpdateScreenScroll();

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
    }

    void UnloadLevel(bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        viewport.x = 0;
        viewport.y = 0;
        UpdateScreenScroll();

        // Clear actors
        actors.Clear();

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
    }

    void ReloadLevel(bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        UnloadLevel(refresh);

        for (u32 i = 0; i < pCurrentLevel->actors.Count(); i++)
        {
            auto handle = pCurrentLevel->actors.GetHandle(i);
            const Actor* pActor = pCurrentLevel->actors.Get(handle);

            auto spawnedHandle = SpawnActor(pActor);
            InitializeActor(spawnedHandle);
        }

        gameplaySecondsElapsed = 0.0f;

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
    }

    void Initialize() {
        // Rendering data
        pRenderSettings = Rendering::GetSettingsPtr();
        pChr = Rendering::GetChrPtr(0);
        pPalettes = Rendering::GetPalettePtr(0);
        pSprites = Rendering::GetSpritesPtr(0);
        pNametables = Rendering::GetNametablePtr(0);
        pScanlines = Rendering::GetScanlinePtr(0);

        // Init chr memory
        // TODO: Pre-process these instead of loading from bitmap at runtime!
        ChrSheet temp;
        Rendering::Util::CreateChrSheet("assets/chr000.bmp", &temp);
        Rendering::Util::CopyChrTiles(temp.tiles, pChr[0].tiles, CHR_SIZE_TILES);
        Rendering::Util::CreateChrSheet("assets/chr001.bmp", &temp);
        Rendering::Util::CopyChrTiles(temp.tiles, pChr[1].tiles, CHR_SIZE_TILES);

        Rendering::Util::CreateChrSheet("assets/player.bmp", &playerBank);

        //u8 paletteColors[8 * 8];
        Rendering::Util::LoadPaletteColorsFromFile("assets/palette.dat", basePaletteColors);

        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            memcpy(pPalettes, basePaletteColors, PALETTE_MEMORY_SIZE);
        }

        Rendering::Util::ClearSprites(pSprites, MAX_SPRITE_COUNT);

        Tiles::LoadTileset("assets/forest.til");
        Metasprites::Load("assets/meta.spr");
        Levels::Init();
        Levels::LoadLevels("assets/levels.lev");
        Actors::LoadPrototypes("assets/actors.prt");

        // Initialize scanline state
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = { 0, 0 };
        }

        viewport.x = 0.0f;
        viewport.y = 0.0f;

        actors.Init(512);
        actorRemoveList.Init(512);

        // TEMP SOUND STUFF
        jumpSfx = Audio::LoadSound("assets/jump.nsf");
        gunSfx = Audio::LoadSound("assets/gun1.nsf");
        ricochetSfx = Audio::LoadSound("assets/ricochet.nsf");

        // TODO: Level should load palettes and tileset?
        LoadLevel(0);
    }

    void Free() {
        Audio::FreeSound(&jumpSfx);
        Audio::FreeSound(&gunSfx);
        Audio::FreeSound(&ricochetSfx);
    }

    void Step(r64 dt) {
        secondsElapsed += dt;

        static Sprite* pNextSprite = pSprites;

        const u32 spritesToClear = pNextSprite - pSprites;
        Rendering::Util::ClearSprites(pSprites, spritesToClear);
        pNextSprite = pSprites;

        if (!paused) {
            gameplaySecondsElapsed += dt;

            UpdateActors(&pNextSprite, dt);

            UpdateViewport();
        }

        UpdateScreenScroll();

        // Animate color palette brightness
        /*r32 deltaBrightness = sin(gameplaySecondsElapsed * 1.5f);
        for (u32 i = 0; i < Rendering::paletteCount * Rendering::paletteColorCount; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 brightness = (baseColor & 0b1110000) >> 4;
            s32 d = (s32)roundf(deltaBrightness * 8);

            s32 newBrightness = brightness + d;
            newBrightness = (newBrightness < 0) ? 0 : (newBrightness > 7) ? 7 : newBrightness;

            u8 newColor = (baseColor & 0b0001111) | (newBrightness << 4);
            ((u8*)pPalettes)[i] = newColor;
        }*/

        // Animate color palette hue
        /*s32 hueShift = (s32)roundf(gameplaySecondsElapsed * 5.0f);
        for (u32 i = 0; i < Rendering::paletteCount * Rendering::paletteColorCount; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 hue = baseColor & 0b1111;

            s32 newHue = hue + hueShift;
            newHue &= 0b1111;

            u8 newColor = (baseColor & 0b1110000) | newHue;
            ((u8*)pPalettes)[i] = newColor;
        }*/

    }

    bool IsPaused() {
        return paused;
    }
    void SetPaused(bool p) {
        paused = p;
    }

    Viewport* GetViewport() {
        return &viewport;
    }
    Level* GetLevel() {
        return pCurrentLevel;
    }
    Pool<Actor>* GetActors() {
        return &actors;
    }
#pragma endregion
}