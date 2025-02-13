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
#include "nes_timing.h"

namespace Game {
    r64 secondsElapsed = 0.0f;
    u32 clockCounter = 0;

    // 16ms Frames elapsed while not paused
    u32 gameplayFramesElapsed = 0;

    // Input
    u8 currentInput = BUTTON_NONE;
    u8 previousInput = BUTTON_NONE;

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

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;

    //Sound bgm;
    //bool musicPlaying = false;

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

#pragma region Input
    static bool ButtonDown(u8 flags) {
        return Input::ButtonDown(flags, currentInput);
    }

    static bool ButtonUp(u8 flags) {
        return Input::ButtonUp(flags, currentInput);
    }

    static bool ButtonPressed(u8 flags) {
        return Input::ButtonPressed(flags, currentInput, previousInput);
    }

    static bool ButtonReleased(u8 flags) {
        return Input::ButtonReleased(flags, currentInput, previousInput);
    }
#

#pragma region Viewport
    static void UpdateScreenScroll() {
        // Drugs mode
        /*for (int i = 0; i < 288; i++) {
            float sine = sin(gameplayFramesElapsed / 60.f + (i / 16.0f));
            const Scanline state = {
                (s32)((viewport.x + sine / 4) * METATILE_DIM_PIXELS),
                (s32)(viewport.y * METATILE_DIM_PIXELS)
            };
            pScanlines[i] = state;
        }*/

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
        pActor->frameIndex = 0;
        pActor->animCounter = pActor->animFrameLength;
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
    static bool UpdateCounter(s32& counter) {
        if (counter == 0) {
            return false;
        }

        counter--;
        return true;
    }
#pragma endregion

#pragma region Rendering
    static void DrawActor(const Actor* pActor, Sprite** ppNextSprite, s32 frameIndex = -1, const IVec2& pixelOffset = {0,0}, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1) {
        // Culling
        if (!PositionInViewportBounds(pActor->position)) {
            return;
        }

        IVec2 drawPos = WorldPosToScreenPixels(pActor->position) + pixelOffset;
        const ActorAnimFrame& frame = pActor->pPrototype->frames[frameIndex == -1 ? pActor->frameIndex : frameIndex];

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
        return (pActor->damageCounter > 0) ? (gameplayFramesElapsed / 3) % 4 : -1;
    }

    static s32 GetAnimFrameFromDirection(const Vec2& dir, u32 frameCount) {
        const r32 angle = atan2f(dir.y, dir.x);
        return (s32)roundf(((angle + pi) / (pi * 2)) * frameCount) % frameCount;
    }

    static void AdvanceAnimation(Actor* pActor, bool loop = true, s32 frameCountOverride = -1, s32 frameLengthOverride = -1) {
        const u32 frameLength = frameLengthOverride == -1 ? pActor->animFrameLength : frameLengthOverride;
        const u32 frameCount = frameCountOverride == -1 ? pActor->pPrototype->frameCount : frameCountOverride;

        if (pActor->animCounter == 0) {
            pActor->frameIndex = ++pActor->frameIndex % frameCount;
            pActor->animCounter = frameLength;
        }
        else if (pActor->frameIndex < frameCount - 1 || loop) {
            pActor->animCounter--;
        }
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

    static bool ActorMoveHorizontal(Actor* pActor, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dx = pActor->velocity.x;

        Collision::SweepBoxHorizontal(&pCurrentLevel->tilemap, hitbox, pActor->position, dx, outHit);
        pActor->position.x = outHit.location.x;
        return outHit.blockingHit;
    }

    static bool ActorMoveVertical(Actor* pActor, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dy = pActor->velocity.y;

        Collision::SweepBoxVertical(&pCurrentLevel->tilemap, hitbox, pActor->position, dy, outHit);
        pActor->position.y = outHit.location.y;
        return outHit.blockingHit;
    }

    static void ApplyGravity(Actor* pActor, r32 gravity) {
        pActor->velocity.y += gravity;
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
        // TODO: Determine lifetime based on prototype anim frame length
        pHit->lifetime = 12;
        pHit->animFrameLength = pHit->lifetime / pHit->pPrototype->frameCount;
        pHit->velocity = Vec2{};
        InitializeActor(pHit);
    }

    static void ActorDie(Actor* pActor, const Vec2& explosionPos) {
        pActor->flags.pendingRemoval = true;
        SpawnExplosion(explosionPos, pActor->pPrototype->deathEffect);
    }

    static bool ActorTakeDamage(Actor* pActor, u32 damage) {
        constexpr s32 damageDelay = 30;

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

            pDmg->lifetime = 60;
            pDmg->velocity = { 0, -0.03125f };

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
            pPlayer->velocity.x = -0.0625f;
        }
        else {
            pPlayer->flags.facingDir = -1;
            pPlayer->velocity.x = 0.0625f;
        }

    }

    static void PlayerInput(Actor* pPlayer) {
        PlayerState& playerState = pPlayer->playerState;
        if (ButtonDown(BUTTON_DPAD_LEFT)) {
            pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
            pPlayer->velocity.x = -0.125;
        }
        else if (ButtonDown(BUTTON_DPAD_RIGHT)) {
            pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
            pPlayer->velocity.x = 0.125f;
        }
        else {
            pPlayer->velocity.x = 0;
        }

        // Aim mode
        if (ButtonDown(BUTTON_DPAD_UP)) {
            playerState.aimMode = PLAYER_AIM_UP;
        }
        else if (ButtonDown(BUTTON_DPAD_DOWN)) {
            playerState.aimMode = PLAYER_AIM_DOWN;
        }
        else playerState.aimMode = PLAYER_AIM_FWD;

        if (ButtonPressed(BUTTON_START)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if (ButtonPressed(BUTTON_A) && (!pPlayer->flags.inAir || !playerState.doubleJumped)) {
            pPlayer->velocity.y = -0.25f;
            if (pPlayer->flags.inAir) {
                playerState.doubleJumped = true;
            }

            // Trigger new flap by taking wings out of falling position by advancing the frame index
            pPlayer->frameIndex = ++pPlayer->frameIndex % PLAYER_WING_FRAME_COUNT;

            Audio::PlaySFX(&jumpSfx, CHAN_ID_PULSE0);
        }

        if (pPlayer->velocity.y < 0 && ButtonReleased(BUTTON_A)) {
            pPlayer->velocity.y /= 2;
        }

        if (ButtonDown(BUTTON_A) && pPlayer->velocity.y > 0) {
            playerState.slowFall = true;
        }

        if (ButtonReleased(BUTTON_B)) {
            playerState.shootCounter = 0.0f;
        }

        if (ButtonPressed(BUTTON_SELECT)) {
            if (playerState.weapon == PLAYER_WEAPON_LAUNCHER) {
                playerState.weapon = PLAYER_WEAPON_BOW;
            }
            else playerState.weapon = PLAYER_WEAPON_LAUNCHER;
        }
    }

    static void PlayerShoot(Actor* pPlayer) {
        constexpr s32 shootDelay = 10;

        PlayerState& playerState = pPlayer->playerState;
        UpdateCounter(playerState.shootCounter);

        if (ButtonDown(BUTTON_B) && playerState.shootCounter == 0) {
            playerState.shootCounter = shootDelay;

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
            pBullet->gravity = 0.04;
            pBullet->lifetime = 60;

            constexpr r32 bulletVel = 0.625f;
            constexpr r32 bulletVelSqrt2 = 0.45f; // vel / sqrt(2)

            if (playerState.aimMode == PLAYER_AIM_FWD) {
                pBullet->position = pBullet->position + fwdOffset;
                pBullet->velocity.x = bulletVel * pPlayer->flags.facingDir;
            }
            else {
                pBullet->velocity.x = bulletVelSqrt2 * pPlayer->flags.facingDir;
                pBullet->velocity.y = (playerState.aimMode == PLAYER_AIM_UP) ? -bulletVelSqrt2 : bulletVelSqrt2;
                pBullet->position = pBullet->position + ((playerState.aimMode == PLAYER_AIM_UP) ? upOffset : downOffset);
            }

            if (playerState.weapon == PLAYER_WEAPON_LAUNCHER) {
                pBullet->velocity = pBullet->velocity * 0.75f;
                Audio::PlaySFX(&gunSfx, CHAN_ID_NOISE);
            }

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

    static void DrawPlayer(Actor* pPlayer, Sprite** ppNextSprite) {
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
        const bool wingsInPosition = (jumping && pPlayer->frameIndex == PLAYER_WINGS_ASCEND) || (falling && pPlayer->frameIndex == PLAYER_WINGS_DESCEND);

        // Wings flap faster to get into proper position
        const r32 wingAnimFrameLength = (jumping || falling) ? pPlayer->animFrameLength : pPlayer->animFrameLength * 2;

        if (!wingsInPosition) {
            AdvanceAnimation(pPlayer, true, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength);
        }

        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerWingFrameBankOffsets[pPlayer->frameIndex],
            pChr[1].tiles + playerWingFrameChrOffset,
            playerWingFrameTileCount
        );

        // Setup draw data
        s32 vOffset = 0;
        if (pPlayer->velocity.y == 0) {
            vOffset = pPlayer->frameIndex > PLAYER_WINGS_FLAP_START ? -1 : 0;
        }

        DrawPlayerGun(pPlayer, vOffset, ppNextSprite);
        const s32 paletteOverride = GetDamagePaletteOverride(pPlayer);
        DrawActor(pPlayer, ppNextSprite, playerState.aimMode, { 0, vOffset }, pPlayer->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }

    static void UpdatePlayer(Actor* pActor, Sprite** ppNextSprite) {
        UpdateCounter(pActor->damageCounter);
        
        // Reset slow fall
        pActor->playerState.slowFall = false;

        const bool playerStunned = pActor->damageCounter > 0;
        if (!playerStunned) {
            PlayerInput(pActor);
            PlayerShoot(pActor);
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            pActor->velocity.x = 0.0f;
        }

        const r32 gravity = pActor->playerState.slowFall ? pActor->gravity / 4 : pActor->gravity;
        ApplyGravity(pActor, gravity);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, hit)) {
            pActor->velocity.y = 0.0f;

            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
                pActor->playerState.doubleJumped = false;
            }
        }

        // TODO: collision with other actors

        DrawPlayer(pActor, ppNextSprite);
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

    static void UpdateBullet(Actor* pActor, Sprite** ppNextSprite) {
        if (!UpdateCounter(pActor->lifetimeCounter)) {
            ActorDie(pActor, pActor->position);
            return;
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            ActorDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, hit)) {
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

    static void UpdateBouncyBullet(Actor* pActor, Sprite** ppNextSprite) {
        if (!UpdateCounter(pActor->lifetimeCounter)) {
            ActorDie(pActor, pActor->position);
        }

        ApplyGravity(pActor, pActor->gravity);

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        if (ActorMoveVertical(pActor, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        ForEachActorCollision(pActor, ACTOR_COLLISION_LAYER_ENEMY, HandleBulletEnemyCollision);

        const u32 frameCount = pActor->pPrototype->frameCount;
        const s32 frameIndex = GetAnimFrameFromDirection(pActor->velocity.Normalize(), frameCount);

        DrawActor(pActor, ppNextSprite, frameIndex);
    }
#pragma endregion

#pragma region Enemy bullets
    static void UpdateEnemyFireball(Actor* pActor, Sprite** ppNextSprite) {
        if (!UpdateCounter(pActor->lifetimeCounter)) {
            ActorDie(pActor, pActor->position);
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            ActorDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, hit)) {
            ActorDie(pActor, hit.impactPoint);
            return;
        }

        Actor* pPlayer = nullptr;
        if (ActorCollidesWithPlayer(pActor, &pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
            ActorDie(pActor, pActor->position);
        }

        AdvanceAnimation(pActor);

        DrawActor(pActor, ppNextSprite, pActor->frameIndex);
    }
#pragma endregion

#pragma region Enemies
    static void UpdateSlimeEnemy(Actor* pActor, Sprite** ppNextSprite) {
        UpdateCounter(pActor->damageCounter);

        if (!pActor->flags.inAir) {
            const bool shouldJump = (rand() % 128) == 0;
            if (shouldJump) {
                pActor->velocity.y = -0.25f;
                ActorFacePlayer(pActor);
                pActor->velocity.x = 0.15625f * pActor->flags.facingDir;
            }
            else {
                pActor->velocity.x = 0.00625f * pActor->flags.facingDir;
            }
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            pActor->velocity.x = 0.0f;
            pActor->flags.facingDir = (s8)hit.impactNormal.x;
        }

        ApplyGravity(pActor, pActor->gravity);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, hit)) {
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

    static void UpdateSkullEnemy(Actor* pActor, Sprite** ppNextSprite) {
        UpdateCounter(pActor->damageCounter);

        ActorFacePlayer(pActor);

        static const r32 amplitude = 4.0f;
        const r32 sineTime = sin(gameplayFramesElapsed / 60.f);
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
                pBullet->lifetime = 600;
                const Vec2 playerDir = (pPlayer->position - pActor->position).Normalize();
                pBullet->velocity = playerDir * 0.0625f;

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
    static void UpdateEffect(Actor* pActor) {
        if (!UpdateCounter(pActor->lifetimeCounter)) {
            pActor->flags.pendingRemoval = true;
        }
    }

    static void UpdateExplosion(Actor* pActor, Sprite** ppNextSprite) {
        UpdateEffect(pActor);

        const u32 frameCount = pActor->pPrototype->frameCount;

        AdvanceAnimation(pActor, false);

        DrawActor(pActor, ppNextSprite);
    }

    static void UpdateNumbers(Actor* pActor, Sprite** ppNextSprite) {
        UpdateEffect(pActor);

        pActor->position.y += pActor->velocity.y;

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

    static void UpdateActors(Sprite** ppNextSprite) {
        
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
                UpdatePlayer(pActor, ppNextSprite);
                break;
            }
            // Player projectiles
            case ACTOR_BEHAVIOUR_BULLET: {
                UpdateBullet(pActor, ppNextSprite);
                break;
            }
            case ACTOR_BEHAVIOUR_BULLET_BOUNCY: {
                UpdateBouncyBullet(pActor, ppNextSprite);
                break;
            }
            // Enemy projectiles
            case ACTOR_BEHAVIOUR_FIREBALL: {
                UpdateEnemyFireball(pActor, ppNextSprite);
                break;
            }
            // Enemies
            case ACTOR_BEHAVIOUR_ENEMY_SLIME: {
                UpdateSlimeEnemy(pActor, ppNextSprite);
                break;
            }
            case ACTOR_BEHAVIOUR_ENEMY_SKULL: {
                UpdateSkullEnemy(pActor, ppNextSprite);
                break;
            }
            // Effects
            case ACTOR_BEHAVIOUR_FX_EXPLOSION: {
                UpdateExplosion(pActor, ppNextSprite);
                break;
            }
            case ACTOR_BEHAVIOUR_FX_NUMBERS: {
                UpdateNumbers(pActor, ppNextSprite);
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

    static void Step() {
        previousInput = currentInput;
        currentInput = Input::GetControllerState();

        static Sprite* pNextSprite = pSprites;

        const u32 spritesToClear = pNextSprite - pSprites;
        Rendering::Util::ClearSprites(pSprites, spritesToClear);
        pNextSprite = pSprites;

        if (!paused) {
            gameplayFramesElapsed++;

            UpdateActors(&pNextSprite);

            UpdateViewport();
        }

        UpdateScreenScroll();

        /*if (ButtonPressed(BUTTON_START)) {
            if (!musicPlaying) {
                Audio::PlayMusic(&bgm, true);
            }
            else {
                Audio::StopMusic();
            }
            musicPlaying = !musicPlaying;
        }*/

        // Animate color palette brightness
        /*r32 deltaBrightness = sin(gameplayFramesElapsed / 40.f);
        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 brightness = (baseColor & 0b1110000) >> 4;
            s32 d = (s32)roundf(deltaBrightness * 8);

            s32 newBrightness = brightness + d;
            newBrightness = (newBrightness < 0) ? 0 : (newBrightness > 7) ? 7 : newBrightness;

            u8 newColor = (baseColor & 0b0001111) | (newBrightness << 4);
            ((u8*)pPalettes)[i] = newColor;
        }*/

        // Animate color palette hue
        /*s32 hueShift = (s32)roundf(gameplayFramesElapsed / 12.f);
        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 hue = baseColor & 0b1111;

            s32 newHue = hue + hueShift;
            newHue &= 0b1111;

            u8 newColor = (baseColor & 0b1110000) | newHue;
            ((u8*)pPalettes)[i] = newColor;
        }*/
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

        gameplayFramesElapsed = 0;

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
        //bgm = Audio::LoadSound("assets/music.nsf");

        // TODO: Level should load palettes and tileset?
        LoadLevel(0);
    }

    void Free() {
        Audio::StopMusic();

        Audio::FreeSound(&jumpSfx);
        Audio::FreeSound(&gunSfx);
        Audio::FreeSound(&ricochetSfx);
        //Audio::FreeSound(&bgm);
    }

    void Update(r64 dt) {
        secondsElapsed += dt;
        while (secondsElapsed >= CLOCK_PERIOD) {
            clockCounter++;
            secondsElapsed -= CLOCK_PERIOD;

            // One frame
            if (clockCounter == FRAME_CLOCK) {
                Step();
                clockCounter = 0;
            }
        }
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