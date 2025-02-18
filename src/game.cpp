#include "game.h"
#include "system.h"
#include "input.h"
#include <cstring>
#include <cstdio>
#include "rendering_util.h"
#include "level.h"
#include "viewport.h"
#include "collision.h"
#include "metasprite.h"
#include "tiles.h"
#include <imgui.h>
#include <vector>
#include "audio.h"
#include "nes_timing.h"
#include <gtc/constants.hpp>

// TODO: Move somewhere else?
enum SpriteLayerType : u8 {
    SPRITE_LAYER_UI,
    SPRITE_LAYER_FX,
    SPRITE_LAYER_FG,
    SPRITE_LAYER_BG,

    SPRITE_LAYER_COUNT
};

struct SpriteLayer {
    Sprite* pNextSprite = nullptr;
    u32 spriteCount = 0;
};

constexpr u32 LAYER_SPRITE_COUNT = MAX_SPRITE_COUNT / SPRITE_LAYER_COUNT;

static void ClearSpriteLayers(SpriteLayer* layers, bool fullClear = false) {
    const Sprite* pSprites = Rendering::GetSpritesPtr(0);

    for (u32 i = 0; i < SPRITE_LAYER_COUNT; i++) {
        SpriteLayer& layer = layers[i];

        u32 beginIndex = i << 10;
        Sprite* pBeginSprite = Rendering::GetSpritesPtr(beginIndex);

        const u32 spritesToClear = fullClear ? LAYER_SPRITE_COUNT : layer.spriteCount;
        Rendering::Util::ClearSprites(pBeginSprite, spritesToClear);
        layer.pNextSprite = pBeginSprite;
        layer.spriteCount = 0;
    }
}

static Sprite* GetNextFreeSprite(SpriteLayer* pLayer, u32 count = 1) {
    if (pLayer->spriteCount + count > LAYER_SPRITE_COUNT) {
        return nullptr;
    }

    Sprite* result = pLayer->pNextSprite;
    pLayer->spriteCount += count;
    pLayer->pNextSprite += count;

    return result;
}

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
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    // Sprites
    SpriteLayer spriteLayers[SPRITE_LAYER_COUNT];

    // Viewport
    Viewport viewport;

    Level* pCurrentLevel = nullptr;

    Pool<Actor> actors;
    Pool<PoolHandle<Actor>> actorRemoveList;

    // Global player stuff
    PoolHandle<Actor> playerHandle;
    u16 playerHealth = 10;
    u8 playerWeapon = PLAYER_WEAPON_LAUNCHER;

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

    constexpr glm::ivec2 playerBowOffsets[3] = { { 10, -4 }, { 9, -14 }, { 10, 4 } };
    constexpr u32 playerBowFwdMetaspriteIndex = 8;
    constexpr u32 playerBowDiagMetaspriteIndex = 9;
    constexpr u32 playerBowArrowFwdMetaspriteIndex = 3;
    constexpr u32 playerBowArrowDiagMetaspriteIndex = 4;

    constexpr glm::ivec2 playerLauncherOffsets[3] = { { 5, -5 }, { 7, -12 }, { 8, 1 } };
    constexpr u32 playerLauncherFwdMetaspriteIndex = 10;
    constexpr u32 playerLauncherDiagMetaspriteIndex = 11;
    constexpr u32 playerLauncherGrenadeMetaspriteIndex = 12;

    constexpr glm::vec2 viewportScrollThreshold = { 4.0f, 3.0f };

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
            float sine = glm::sin(gameplayFramesElapsed / 60.f + (i / 16.0f));
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

        glm::vec2 viewportCenter = glm::vec2{ viewport.x + VIEWPORT_WIDTH_METATILES / 2.0f, viewport.y + VIEWPORT_HEIGHT_METATILES / 2.0f };
        glm::vec2 targetOffset = pPlayer->position - viewportCenter;

        glm::vec2 delta = { 0.0f, 0.0f };
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

    static bool PositionInViewportBounds(glm::vec2 pos) {
        return pos.x >= viewport.x &&
            pos.x < viewport.x + VIEWPORT_WIDTH_METATILES &&
            pos.y >= viewport.y &&
            pos.y < viewport.y + VIEWPORT_HEIGHT_METATILES;
    }

    static glm::ivec2 WorldPosToScreenPixels(glm::vec2 pos) {
        return glm::ivec2{
            (s32)glm::roundEven((pos.x - viewport.x) * METATILE_DIM_PIXELS),
            (s32)glm::roundEven((pos.y - viewport.y) * METATILE_DIM_PIXELS)
        };
    }
#pragma endregion

#pragma region Actor initialization
    static void InitializeActor(Actor* pActor) {
        const ActorPrototype* pPrototype = pActor->pPrototype;

        pActor->flags.facingDir = ACTOR_FACING_RIGHT;
        pActor->flags.inAir = true;
        pActor->flags.active = true;
        pActor->flags.pendingRemoval = false;

        pActor->initialPosition = pActor->position;
        pActor->velocity = glm::vec2{};

        pActor->frameIndex = 0;
        pActor->animCounter = pPrototype->animations[0].frameLength;

        switch (pPrototype->type) {
        case ACTOR_TYPE_PLAYER: {
            pActor->playerState.damageCounter = 0;
            break;
        }
        case ACTOR_TYPE_NPC: {
            pActor->npcState.health = pPrototype->npcData.health;
            pActor->npcState.damageCounter = 0;
            break;
        }
        case ACTOR_TYPE_BULLET: {
            pActor->bulletState.lifetime = pPrototype->bulletData.lifetime;
            pActor->bulletState.lifetimeCounter = pPrototype->bulletData.lifetime;
            break;
        }
        case ACTOR_TYPE_PICKUP: {
            break;
        }
        case ACTOR_TYPE_EFFECT: {
            pActor->effectState.lifetime = pPrototype->effectData.lifetime;
            pActor->effectState.lifetimeCounter = pPrototype->effectData.lifetime;
            break;
        }
        default: 
            break;
        }
    }

    static Actor* SpawnActor(const Actor* pTemplate) {
        auto handle = actors.Add(*pTemplate);

        if (handle == PoolHandle<Actor>::Null()) {
            return nullptr;
        }

        if (pTemplate->pPrototype->type == ACTOR_TYPE_PLAYER) {
            playerHandle = handle;
        }

        Actor* pActor = actors.Get(handle);
        InitializeActor(pActor);

        return pActor;
    }

    static Actor* SpawnActor(u32 presetIndex, const glm::vec2& position) {
        auto handle = actors.Add();

        if (handle == PoolHandle<Actor>::Null()) {
            return nullptr;
        }

        Actor* pActor = actors.Get(handle);

        const ActorPrototype* pPrototype = Actors::GetPrototype(presetIndex);
        pActor->pPrototype = pPrototype;
        if (pPrototype->type == ACTOR_TYPE_PLAYER) {
            playerHandle = handle;
        }

        pActor->position = position;
        InitializeActor(pActor);

        return pActor;
    }
#pragma endregion

#pragma region Actor utils
    // Returns false if counter stops, true if keeps running
    static bool UpdateCounter(u16& counter) {
        if (counter == 0) {
            return false;
        }

        counter--;
        return true;
    }
#pragma endregion

#pragma region Rendering
    static bool DrawActor(const Actor* pActor, u8 layerIndex = SPRITE_LAYER_FG, const glm::ivec2& pixelOffset = {0,0}, bool hFlip = false, bool vFlip = false, s32 paletteOverride = -1) {
        // Culling
        if (!PositionInViewportBounds(pActor->position)) {
            return false;
        }

        SpriteLayer& layer = spriteLayers[layerIndex];

        glm::ivec2 drawPos = WorldPosToScreenPixels(pActor->position) + pixelOffset;
        const Animation& currentAnim = pActor->pPrototype->animations[0];

        switch (currentAnim.type) {
        case ANIMATION_TYPE_SPRITES: {
            Sprite* outSprite = GetNextFreeSprite(&layer);
            if (outSprite == nullptr) {
                return false;
            }

            const s32 metaspriteIndex = (s32)currentAnim.metaspriteIndex;
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos + pActor->frameIndex, outSprite, 1, drawPos, hFlip, vFlip, paletteOverride);
            break;
        }
        case ANIMATION_TYPE_METASPRITES: {
            const s32 metaspriteIndex = (s32)currentAnim.metaspriteIndex + pActor->frameIndex;
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);

            Sprite* outSprites = GetNextFreeSprite(&layer, pMetasprite->spriteCount);
            if (outSprites == nullptr) {
                return false;
            }

            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos, outSprites, pMetasprite->spriteCount, drawPos, hFlip, vFlip, paletteOverride);
            break;
        }
        default:
            break;
        }

        return true;
    }

    static s32 GetDamagePaletteOverride(u8 damageCounter) {
        return (damageCounter > 0) ? (gameplayFramesElapsed / 3) % 4 : -1;
    }

    static void GetAnimFrameFromDirection(Actor* pActor) {
        const glm::vec2 dir = glm::normalize(pActor->velocity);
        const r32 angle = glm::atan(dir.y, dir.x);

        const Animation& currentAnim = pActor->pPrototype->animations[0];
        pActor->frameIndex = (s32)glm::roundEven(((angle + glm::pi<r32>()) / (glm::pi<r32>() * 2)) * currentAnim.frameCount) % currentAnim.frameCount;
    }

    // General function that can be used to advance "fake" animations like pattern bank streaming
    static void AdvanceAnimation(u16& animCounter, u16& frameIndex, u16 frameCount, u8 frameLength, s16 loopPoint) {
        const bool loop = loopPoint != -1;
        if (animCounter == 0) {
            // End of anim reached
            if (frameIndex == frameCount - 1) {
                if (loop) {
                    frameIndex = loopPoint;
                }
                else return;
            }
            else frameIndex++;
            
            animCounter = frameLength;
            return;
        }
        animCounter--;
    }

    static void AdvanceCurrentAnimation(Actor* pActor) {
        const Animation& currentAnim = pActor->pPrototype->animations[0];
        AdvanceAnimation(pActor->animCounter, pActor->frameIndex, currentAnim.frameCount, currentAnim.frameLength, currentAnim.loopPoint);
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
    static void ForEachActorCollision(Actor* pActor, u16 type, u8 alignment, void (*callback)(Actor*, Actor*)) {
        for (u32 i = 0; i < actors.Count(); i++)
        {
            if (!pActor->flags.active || pActor->flags.pendingRemoval) {
                break;
            }

            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pOther = actors.Get(handle);

            if (pOther == nullptr || pOther->pPrototype->type != type || pOther->pPrototype->alignment != alignment || pOther->flags.pendingRemoval || !pOther->flags.active) {
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

    static void ApplyGravity(Actor* pActor, r32 gravity = 0.01f) {
        pActor->velocity.y += gravity;
    }
#pragma endregion

#pragma region Damage
    static void NPCDie(Actor* pActor) {
        pActor->flags.pendingRemoval = true;

        SpawnActor(pActor->pPrototype->npcData.spawnOnDeath, pActor->position);
    }

    static bool ActorTakeDamage(Actor* pActor, u32 dmgValue, u16& health, u16& damageCounter) {
        constexpr s32 damageDelay = 30;

        if (dmgValue > health) {
            health = 0;
        }
        else health -= dmgValue;
        damageCounter = damageDelay;

        // Spawn damage numbers
        const AABB& hitbox = pActor->pPrototype->hitbox;
        // Random point inside hitbox
        const glm::vec2 randomPointInsideHitbox = {
            ((r32)rand() / RAND_MAX) * (hitbox.x2 - hitbox.x1) + hitbox.x1,
            ((r32)rand() / RAND_MAX) * (hitbox.y2 - hitbox.y1) + hitbox.y1
        };
        const glm::vec2 spawnPos = pActor->position + randomPointInsideHitbox;

        Actor* pDmg = SpawnActor(dmgNumberPrototypeIndex, spawnPos);
        if (pDmg != nullptr) {
            pDmg->effectState.value = -dmgValue;
            pDmg->velocity = { 0, -0.03125f };
        }

        if (health <= 0) {
            return false;
        }

        return true;
    }
#pragma endregion

#pragma region Player logic
    static void HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy) {
        // If invulnerable
        if (pPlayer->playerState.damageCounter != 0) {
            return;
        }

        const u32 damage = (rand() % 2) + 1;
        if (!ActorTakeDamage(pPlayer, damage, playerHealth, pPlayer->playerState.damageCounter)) {
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
            playerState.flags.aimMode = PLAYER_AIM_UP;
        }
        else if (ButtonDown(BUTTON_DPAD_DOWN)) {
            playerState.flags.aimMode = PLAYER_AIM_DOWN;
        }
        else playerState.flags.aimMode = PLAYER_AIM_FWD;

        if (ButtonPressed(BUTTON_START)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if (ButtonPressed(BUTTON_A) && (!pPlayer->flags.inAir || !playerState.flags.doubleJumped)) {
            pPlayer->velocity.y = -0.25f;
            if (pPlayer->flags.inAir) {
                playerState.flags.doubleJumped = true;
            }

            // Trigger new flap by taking wings out of falling position by advancing the frame index
            playerState.wingFrame = ++playerState.wingFrame % PLAYER_WING_FRAME_COUNT;

            Audio::PlaySFX(&jumpSfx, CHAN_ID_PULSE0);
        }

        if (pPlayer->velocity.y < 0 && ButtonReleased(BUTTON_A)) {
            pPlayer->velocity.y /= 2;
        }

        if (ButtonDown(BUTTON_A) && pPlayer->velocity.y > 0) {
            playerState.flags.slowFall = true;
        }

        if (ButtonReleased(BUTTON_B)) {
            playerState.shootCounter = 0.0f;
        }

        if (ButtonPressed(BUTTON_SELECT)) {
            if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
                playerWeapon = PLAYER_WEAPON_BOW;
            }
            else playerWeapon = PLAYER_WEAPON_LAUNCHER;
        }
    }

    static void PlayerShoot(Actor* pPlayer) {
        constexpr s32 shootDelay = 10;

        PlayerState& playerState = pPlayer->playerState;
        UpdateCounter(playerState.shootCounter);

        if (ButtonDown(BUTTON_B) && playerState.shootCounter == 0) {
            playerState.shootCounter = shootDelay;

            const s32 prototypeIndex = playerWeapon == PLAYER_WEAPON_LAUNCHER ? playerGrenadePrototypeIndex : playerArrowPrototypeIndex;
            Actor* pBullet = SpawnActor(prototypeIndex, pPlayer->position);
            if (pBullet == nullptr) {
                return;
            }

            const glm::vec2 fwdOffset = glm::vec2{ 0.375f * pPlayer->flags.facingDir, -0.25f };
            const glm::vec2 upOffset = glm::vec2{ 0.1875f * pPlayer->flags.facingDir, -0.5f };
            const glm::vec2 downOffset = glm::vec2{ 0.25f * pPlayer->flags.facingDir, -0.125f };

            constexpr r32 bulletVel = 0.625f;
            constexpr r32 bulletVelSqrt2 = 0.45f; // vel / sqrt(2)

            if (playerState.flags.aimMode == PLAYER_AIM_FWD) {
                pBullet->position = pBullet->position + fwdOffset;
                pBullet->velocity.x = bulletVel * pPlayer->flags.facingDir;
            }
            else {
                pBullet->velocity.x = bulletVelSqrt2 * pPlayer->flags.facingDir;
                pBullet->velocity.y = (playerState.flags.aimMode == PLAYER_AIM_UP) ? -bulletVelSqrt2 : bulletVelSqrt2;
                pBullet->position = pBullet->position + ((playerState.flags.aimMode == PLAYER_AIM_UP) ? upOffset : downOffset);
            }

            if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
                pBullet->velocity = pBullet->velocity * 0.75f;
                Audio::PlaySFX(&gunSfx, CHAN_ID_NOISE);
            }

        }
    }

    static bool DrawPlayerGun(Actor* pPlayer, r32 vOffset) {
        glm::ivec2 drawPos = WorldPosToScreenPixels(pPlayer->position);
        drawPos.y += vOffset;

        // Draw weapon first
        glm::ivec2 weaponOffset;
        u8 weaponFrameBankOffset;
        u32 weaponMetaspriteIndex;
        switch (playerWeapon) {
        case PLAYER_WEAPON_BOW: {
            weaponOffset = playerBowOffsets[pPlayer->playerState.flags.aimMode];
            weaponFrameBankOffset = playerBowFrameBankOffsets[pPlayer->playerState.flags.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.flags.aimMode == PLAYER_AIM_FWD ? playerBowFwdMetaspriteIndex : playerBowDiagMetaspriteIndex;
            break;
        }
        case PLAYER_WEAPON_LAUNCHER: {
            weaponOffset = playerLauncherOffsets[pPlayer->playerState.flags.aimMode];
            weaponFrameBankOffset = playerLauncherFrameBankOffsets[pPlayer->playerState.flags.aimMode];
            weaponMetaspriteIndex = pPlayer->playerState.flags.aimMode == PLAYER_AIM_FWD ? playerLauncherFwdMetaspriteIndex : playerLauncherDiagMetaspriteIndex;
            break;
        }
        default:
            break;
        }
        weaponOffset.x *= pPlayer->flags.facingDir;

        Rendering::Util::CopyChrTiles(playerBank.tiles + weaponFrameBankOffset, pChr[1].tiles + playerWeaponFrameChrOffset, playerWeaponFrameTileCount);

        const Metasprite* bowMetasprite = Metasprites::GetMetasprite(weaponMetaspriteIndex);

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_FG];
        Sprite* outSprites = GetNextFreeSprite(&layer, bowMetasprite->spriteCount);
        if (outSprites == nullptr) {
            return false;
        }

        Rendering::Util::CopyMetasprite(bowMetasprite->spritesRelativePos, outSprites, bowMetasprite->spriteCount, drawPos + weaponOffset, pPlayer->flags.facingDir == ACTOR_FACING_LEFT, false);

        return true;
    }

    static void DrawPlayer(Actor* pPlayer) {
        PlayerState& playerState = pPlayer->playerState;

        // Animate chr sheet using player bank
        const bool jumping = pPlayer->velocity.y < 0;
        const bool descending = !jumping && pPlayer->velocity.y > 0;
        const bool falling = descending && !playerState.flags.slowFall;
        const bool moving = glm::abs(pPlayer->velocity.x) > 0;
        const bool takingDamage = playerState.damageCounter > 0;

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
            playerBank.tiles + playerHeadFrameBankOffsets[playerState.flags.aimMode * 4 + headFrameIndex],
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
        const u16 wingAnimFrameLength = (jumping || falling) ? 6 : 12;

        if (!wingsInPosition) {
            AdvanceAnimation(playerState.wingCounter, playerState.wingFrame, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength, 0);
        }

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

        DrawPlayerGun(pPlayer, vOffset);
        const s32 paletteOverride = GetDamagePaletteOverride(playerState.damageCounter);
        pPlayer->frameIndex = playerState.flags.aimMode;
        DrawActor(pPlayer, SPRITE_LAYER_FG, { 0, vOffset }, pPlayer->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }

    static void UpdatePlayerSidescroller(Actor* pActor) {
        UpdateCounter(pActor->playerState.damageCounter);
        
        // Reset slow fall
        pActor->playerState.flags.slowFall = false;

        const bool playerStunned = pActor->playerState.damageCounter > 0;
        if (!playerStunned) {
            PlayerInput(pActor);
            PlayerShoot(pActor);
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            pActor->velocity.x = 0.0f;
        }

        constexpr r32 playerGravity = 0.01f;
        constexpr r32 playerSlowGravity = playerGravity / 4;

        const r32 gravity = pActor->playerState.flags.slowFall ? playerSlowGravity : playerGravity;
        ApplyGravity(pActor, gravity);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, hit)) {
            pActor->velocity.y = 0.0f;

            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
                pActor->playerState.flags.doubleJumped = false;
            }
        }

        DrawPlayer(pActor);
    }
#pragma endregion

#pragma Bullets
    static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
        pBullet->flags.pendingRemoval = true;
        SpawnActor(pBullet->pPrototype->bulletData.spawnOnDeath, effectPos);
    }

    static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
        BulletDie(pBullet, pBullet->position);

        const u32 damage = (rand() % 2) + 1;
        if (!ActorTakeDamage(pEnemy, damage, pEnemy->npcState.health, pEnemy->npcState.damageCounter)) {
            NPCDie(pEnemy);
        }
    }

    static void BulletCollision(Actor* pActor) {
        if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
            ForEachActorCollision(pActor, ACTOR_TYPE_NPC, ACTOR_ALIGNMENT_HOSTILE, HandleBulletEnemyCollision);
        }
        else if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE) {
            Actor* pPlayer = nullptr;
            if (ActorCollidesWithPlayer(pActor, &pPlayer)) {
                HandlePlayerEnemyCollision(pPlayer, pActor);
                BulletDie(pActor, pActor->position);
            }
            // TODO: Collision with friendly NPC:s? Does this happen in the game?
        }
    }

    static void UpdateDefaultBullet(Actor* pActor) {
        if (!UpdateCounter(pActor->bulletState.lifetimeCounter)) {
            BulletDie(pActor, pActor->position);
            return;
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        BulletCollision(pActor);

        GetAnimFrameFromDirection(pActor);
        DrawActor(pActor);
    }

    static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
        velocity = glm::reflect(velocity, normal);
        Audio::PlaySFX(&ricochetSfx, CHAN_ID_PULSE1);
    }

    static void UpdateGrenade(Actor* pActor) {
        if (!UpdateCounter(pActor->bulletState.lifetimeCounter)) {
            BulletDie(pActor, pActor->position);
            return;
        }

        constexpr r32 grenadeGravity = 0.04f;
        ApplyGravity(pActor, grenadeGravity);

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        if (ActorMoveVertical(pActor, hit)) {
            BulletRicochet(pActor->velocity, hit.impactNormal);
        }

        BulletCollision(pActor);

        GetAnimFrameFromDirection(pActor);
        DrawActor(pActor);
    }

    static void UpdateFireball(Actor* pActor) {
        if (!UpdateCounter(pActor->bulletState.lifetimeCounter)) {
            BulletDie(pActor, pActor->position);
            return;
        }

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        if (ActorMoveVertical(pActor, hit)) {
            BulletDie(pActor, hit.impactPoint);
            return;
        }

        BulletCollision(pActor);

        AdvanceCurrentAnimation(pActor);

        DrawActor(pActor);
    }
#pragma endregion

#pragma region NPC
    static void UpdateSlimeEnemy(Actor* pActor) {
        UpdateCounter(pActor->npcState.damageCounter);

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

        ApplyGravity(pActor);

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

        const s32 paletteOverride = GetDamagePaletteOverride(pActor->npcState.damageCounter);
        DrawActor(pActor, SPRITE_LAYER_FG, {0,0}, pActor->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }

    static void UpdateSkullEnemy(Actor* pActor) {
        UpdateCounter(pActor->npcState.damageCounter);

        ActorFacePlayer(pActor);

        static const r32 amplitude = 4.0f;
        const r32 sineTime = glm::sin(gameplayFramesElapsed / 60.f);
        pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

        // Shoot fireballs
        const bool shouldFire = (rand() % 128) == 0;
        if (shouldFire) {

            Actor* pPlayer = actors.Get(playerHandle);
            if (pPlayer != nullptr) {
                Actor* pBullet = SpawnActor(enemyFireballPrototypeIndex, pActor->position);
                if (pBullet == nullptr) {
                    return;
                }

                const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
                pBullet->velocity = playerDir * 0.0625f;
            }
        }

        Actor* pPlayer = nullptr;
        if (ActorCollidesWithPlayer(pActor, &pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        const s32 paletteOverride = GetDamagePaletteOverride(pActor->npcState.damageCounter);
        DrawActor(pActor, SPRITE_LAYER_FG, { 0,0 }, pActor->flags.facingDir == ACTOR_FACING_LEFT, false, paletteOverride);
    }
#pragma endregion

#pragma region Effects
    static void UpdateDefaultEffect(Actor* pActor) {
        if (!UpdateCounter(pActor->effectState.lifetimeCounter)) {
            pActor->flags.pendingRemoval = true;
        }
    }

    static void UpdateExplosion(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        AdvanceCurrentAnimation(pActor);
        DrawActor(pActor, SPRITE_LAYER_FX);
    }

    // Calls itoa, but adds a plus sign if value is positive
    static u32 ItoaSigned(s16 value, char* str) {
        s32 i = 0;
        if (value > 0) {
            str[i++] = '+';
        }

        itoa(value, str + i, 10);

        return strlen(str);
    }

    static void UpdateNumbers(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        pActor->position.y += pActor->velocity.y;

        static char numberStr[16]{};
        const u32 strLength = ItoaSigned(pActor->effectState.value, numberStr);

        // Ascii character '*' = 0x2A
        // There are a couple extra characters (star, comma, period) that could be used for special symbols
        constexpr u8 chrOffset = 0x2A;

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_FX];

        const Animation& currentAnim = pActor->pPrototype->animations[0];
        for (u32 c = 0; c < strLength; c++) {
            // TODO: What about metasprite frames? Handle this more rigorously!
            Sprite* outSprite = GetNextFreeSprite(&layer);
            if (outSprite == nullptr) {
                break;
            }

            const s32 frameIndex = (numberStr[c] - chrOffset) % currentAnim.frameCount;
            const u8 tileId = Metasprites::GetMetasprite(currentAnim.metaspriteIndex)->spritesRelativePos[frameIndex].tileId;

            const glm::ivec2 pixelPos = WorldPosToScreenPixels(pActor->position);
            *outSprite = {
                u16(pixelPos.y),
                u16(pixelPos.x + c * 5),
                tileId,
                1
            };
        }
    }
#pragma endregion
    static void UpdatePlayer(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        case PLAYER_SUBTYPE_SIDESCROLLER: {
            UpdatePlayerSidescroller(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateNPC(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        // Enemies
        case NPC_SUBTYPE_ENEMY_SLIME: {
            UpdateSlimeEnemy(pActor);
            break;
        }
        case NPC_SUBTYPE_ENEMY_SKULL: {
            UpdateSkullEnemy(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateBullet(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        case BULLET_SUBTYPE_DEFAULT: {
            UpdateDefaultBullet(pActor);
            break;
        }
        case BULLET_SUBTYPE_GRENADE: {
            UpdateGrenade(pActor);
            break;
        }
        case BULLET_SUBTYPE_FIREBALL: {
            UpdateFireball(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdatePickup(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        default:
            break;
        }
    }

    static void UpdateEffect(Actor* pActor) {
        switch (pActor->pPrototype->subtype) {
        case EFFECT_SUBTYPE_EXPLOSION: {
            UpdateExplosion(pActor);
            break;
        }
        case EFFECT_SUBTYPE_NUMBERS: {
            UpdateNumbers(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateActors() {
        
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

            switch (pActor->pPrototype->type) {
            case ACTOR_TYPE_PLAYER: {
                UpdatePlayer(pActor);
                break;
            }
            case ACTOR_TYPE_NPC: {
                UpdateNPC(pActor);
                break;
            }
            case ACTOR_TYPE_BULLET: {
                UpdateBullet(pActor);
                break;
            }
            case ACTOR_TYPE_PICKUP: {
                UpdatePickup(pActor);
                break;
            }
            case ACTOR_TYPE_EFFECT: {
                UpdateEffect(pActor);
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

        ClearSpriteLayers(spriteLayers);

        if (!paused) {
            gameplayFramesElapsed++;

            UpdateActors();

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
        /*r32 deltaBrightness = glm::sin(gameplayFramesElapsed / 40.f);
        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            s32 brightness = (baseColor & 0b1110000) >> 4;
            s32 d = (s32)glm::roundEven(deltaBrightness * 8);

            s32 newBrightness = brightness + d;
            newBrightness = (newBrightness < 0) ? 0 : (newBrightness > 7) ? 7 : newBrightness;

            u8 newColor = (baseColor & 0b0001111) | (newBrightness << 4);
            ((u8*)pPalettes)[i] = newColor;
        }*/

        // Animate color palette hue
        /*s32 hueShift = (s32)glm::roundEven(gameplayFramesElapsed / 12.f);
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
        pNametables = Rendering::GetNametablePtr(0);
        pScanlines = Rendering::GetScanlinePtr(0);

        ClearSpriteLayers(spriteLayers, true);

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