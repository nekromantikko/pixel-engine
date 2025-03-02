#include "game.h"
#include "system.h"
#include "game_input.h"
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
#include "random.h"
#include "fixed_hash_map.h"
#include "coroutines.h"
#include "dialog.h"

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

    // Rendering data
    RenderSettings* pRenderSettings;
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    // Sprites
    SpriteLayer spriteLayers[SPRITE_LAYER_COUNT];

    Level* pCurrentLevel = nullptr;

    Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> actors;
    Pool<PoolHandle<Actor>, MAX_DYNAMIC_ACTOR_COUNT> actorRemoveList;

    // Global player stuff
    PoolHandle<Actor> playerHandle;
    u16 playerMaxHealth = 96;
    u16 playerHealth = 96;
    u16 playerDispRedHealth = 96;
    u16 playerDispYellowHealth = 96;
    u16 playerExp = 0;
    u16 playerDispExp = 0;
    u8 playerWeapon = PLAYER_WEAPON_LAUNCHER;
    u16 playerDeathCounter = 0;

    PoolHandle<Actor> interactableHandle;

    struct Checkpoint {
        u16 levelIndex;
        u8 screenIndex;
    };
    Checkpoint lastCheckpoint;

    struct ExpRemnant {
        s32 levelIndex = -1;
        glm::vec2 position;
        u16 value;
    };
    ExpRemnant expRemnant;

    struct PersistedActorState {
        bool dead : 1 = false;
        bool permaDead : 1 = false;
        bool activated : 1 = false;
    };

    FixedHashMap<PersistedActorState> persistedActorStates;

    bool pauseGameplay = false;

    ChrSheet playerBank;

    bool paused = false;

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;
    Sound damageSfx;
    Sound expSfx;
    Sound enemyDieSfx;

    //Sound bgm;
    //bool musicPlaying = false;

    u8 basePaletteColors[PALETTE_MEMORY_SIZE];

    // TODO: Try to eliminate as much of this as possible
    constexpr s32 playerPrototypeIndex = 0;
    constexpr s32 playerGrenadePrototypeIndex = 1;
    constexpr s32 playerArrowPrototypeIndex = 4;
    constexpr s32 dmgNumberPrototypeIndex = 5;
    constexpr s32 enemyFireballPrototypeIndex = 8;
    constexpr s32 haloSmallPrototypeIndex = 0x0a;
    constexpr s32 haloLargePrototypeIndex = 0x0b;
    constexpr s32 xpRemnantPrototypeIndex = 0x0c;
    constexpr s32 featherPrototypeIndex = 0x0e;

    constexpr u8 playerWingFrameBankOffsets[4] = { 0x00, 0x08, 0x10, 0x18 };
    constexpr u8 playerHeadFrameBankOffsets[12] = { 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C };
    constexpr u8 playerLegsFrameBankOffsets[4] = { 0x50, 0x54, 0x58, 0x5C };
    constexpr u8 playerSitBankOffsets[2] = { 0x60, 0x68 };
    constexpr u8 playerDeadBankOffsets[2] = { 0x70, 0x78 };
    constexpr u8 playerBowFrameBankOffsets[3] = { 0x80, 0x88, 0x90 };
    constexpr u8 playerLauncherFrameBankOffsets[3] = { 0xA0, 0xA8, 0xB0 };

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


#pragma region Viewport
    static void UpdateViewport() {
        Actor* pPlayer = actors.Get(playerHandle);
        if (!pPlayer) {
            return;
        }

        const glm::vec2 viewportPos = GetViewportPos();
        const glm::vec2 viewportCenter = viewportPos + glm::vec2{ VIEWPORT_WIDTH_METATILES / 2.0f, VIEWPORT_HEIGHT_METATILES / 2.0f };
        const glm::vec2 targetOffset = pPlayer->position - viewportCenter;

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

        SetViewportPos(viewportPos + delta);
    }

#pragma endregion

#pragma region Actor utils
    // Returns false if counter stops, true if keeps running
    static bool UpdateCounter(u16& counter) {
        if (counter == 0) {
            return false;
        }

        if (--counter == 0) {
            return false;
        }

        return true;
    }

    static u8 GetScreenIndex(const glm::vec2 position) {
        const u32 xScreen = position.x / VIEWPORT_WIDTH_TILES;
        const u32 yScreen = position.y / VIEWPORT_HEIGHT_TILES;

        return xScreen + yScreen * TILEMAP_MAX_DIM_SCREENS;
    }
#pragma endregion

#pragma region Callbacks
    static void ReviveKilledEnemy(u64 id, PersistedActorState& persistedState) {
        persistedState.dead = false;
    }
#pragma endregion

#pragma region Rendering
    static bool DrawSprite(SpriteLayer* pLayer, const Sprite& sprite) {
        Sprite* outSprite = GetNextFreeSprite(pLayer);
        if (outSprite == nullptr) {
            return false;
        }
        *outSprite = sprite;

        return true;
    }

    static void DrawPlayerHealthBar() {
        const u16 xStart = 16;
        const u16 y = 16;

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_UI];

        const u32 totalSegments = playerMaxHealth >> 2;
        const u32 fullRedSegments = playerDispRedHealth >> 2;
        const u32 fullYellowSegments = playerDispYellowHealth >> 2;

        u16 x = xStart;
        for (u32 i = 0; i < totalSegments; i++) {
            const u16 healthDrawn = (i * 4);
            const u16 remainingRedHealth = healthDrawn > playerDispRedHealth ? 0 : playerDispRedHealth - healthDrawn;
            const u16 remainingYellowHealth = healthDrawn > playerDispYellowHealth ? 0 : playerDispYellowHealth - healthDrawn;

            u8 tileId;
            if (i < fullRedSegments) {
                tileId = 0xe4;
            }
            else if (i < fullYellowSegments) {
                tileId = 0xe5 + (remainingRedHealth & 3);
            }
            else {
                if (remainingYellowHealth != remainingRedHealth) {
                    tileId = 0xe8 + (remainingYellowHealth & 3);
                }
                else tileId = 0xe0 + (remainingRedHealth & 3);
            }

            Sprite sprite{};
            sprite.tileId = tileId;
            sprite.palette = 0x1;
            sprite.x = x;
            sprite.y = y;
            DrawSprite(&layer, sprite);

            x += 8;
        }
    }

    static void DrawExpCounter() {
        static char buffer[10];
        snprintf(buffer, sizeof(buffer), "%05u", playerDispExp);
        u32 length = strlen(buffer);

        SpriteLayer& layer = spriteLayers[SPRITE_LAYER_UI];

        const u16 xStart = VIEWPORT_WIDTH_PIXELS - 16 - (length*8);
        const u16 y = 16;

        // Draw halo indicator
        Sprite sprite{};
        sprite.tileId = 0x68;
        sprite.palette = 0x0;
        sprite.x = xStart - 8;
        sprite.y = y;
        DrawSprite(&layer, sprite);

        // Draw counter
        u16 x = xStart;
        for (u32 i = 0; i < length; i++) {
            Sprite sprite{};
            sprite.tileId = 0xd6 + buffer[i] - '0';
            sprite.palette = 0x1;
            sprite.x = x;
            sprite.y = y;
            DrawSprite(&layer, sprite);

            x += 8;
        }
    }

    static bool DrawActor(const Actor* pActor) {
        const ActorDrawState& drawState = pActor->drawState;

        // Culling
        if (!PositionInViewportBounds(pActor->position) || !drawState.visible) {
            return false;
        }

        SpriteLayer& layer = spriteLayers[drawState.layer];

        glm::i16vec2 drawPos = WorldPosToScreenPixels(pActor->position) + drawState.pixelOffset;
        const u16 animIndex = drawState.animIndex % pActor->pPrototype->animCount;
        const Animation& currentAnim = pActor->pPrototype->animations[animIndex];
		const s32 customPalette = drawState.useCustomPalette ? drawState.palette : -1;

        switch (currentAnim.type) {
        case ANIMATION_TYPE_SPRITES: {
            Sprite* outSprite = GetNextFreeSprite(&layer);
            if (outSprite == nullptr) {
                return false;
            }

            const s32 metaspriteIndex = (s32)currentAnim.metaspriteIndex;
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos + drawState.frameIndex, outSprite, 1, drawPos, drawState.hFlip, drawState.vFlip, customPalette);
            break;
        }
        case ANIMATION_TYPE_METASPRITES: {
            const s32 metaspriteIndex = (s32)currentAnim.metaspriteIndex + drawState.frameIndex;
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);

            Sprite* outSprites = GetNextFreeSprite(&layer, pMetasprite->spriteCount);
            if (outSprites == nullptr) {
                return false;
            }

            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos, outSprites, pMetasprite->spriteCount, drawPos, drawState.hFlip, drawState.vFlip, customPalette);
            break;
        }
        default:
            break;
        }

        return true;
    }

    static void SetDamagePaletteOverride(Actor* pActor, u16 damageCounter) {
		if (damageCounter > 0) {
			pActor->drawState.useCustomPalette = true;
			pActor->drawState.palette = (gameplayFramesElapsed / 3) % 4;
		}
		else {
			pActor->drawState.useCustomPalette = false;
		}
    }

    static void GetAnimFrameFromDirection(Actor* pActor) {
        const glm::vec2 dir = glm::normalize(pActor->velocity);
        const r32 angle = glm::atan(dir.y, dir.x);

        const Animation& currentAnim = pActor->pPrototype->animations[0];
        pActor->drawState.frameIndex = (s32)glm::roundEven(((angle + glm::pi<r32>()) / (glm::pi<r32>() * 2)) * currentAnim.frameCount) % currentAnim.frameCount;
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
        AdvanceAnimation(pActor->drawState.animCounter, pActor->drawState.frameIndex, currentAnim.frameCount, currentAnim.frameLength, currentAnim.loopPoint);
    }
#pragma endregion

#pragma region Custom draw functions
    static bool DrawPlayerGun(Actor* pPlayer) {
        const ActorDrawState& drawState = pPlayer->drawState;
        glm::i16vec2 drawPos = WorldPosToScreenPixels(pPlayer->position) + drawState.pixelOffset;

        // Draw weapon first
        glm::i16vec2 weaponOffset;
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

        Rendering::Util::CopyMetasprite(bowMetasprite->spritesRelativePos, outSprites, bowMetasprite->spriteCount, drawPos + weaponOffset, drawState.hFlip, drawState.vFlip);

        return true;
    }

    static void DrawPlayer(Actor* pPlayer) {
        if (playerHealth != 0 && pPlayer->playerState.flags.sitState == PLAYER_STANDING) {
            DrawPlayerGun(pPlayer);
        }
        DrawActor(pPlayer);
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

    static void DrawNumbers(Actor* pActor) {
        const Animation& currentAnim = pActor->pPrototype->animations[pActor->drawState.animIndex];
        if (currentAnim.type != ANIMATION_TYPE_SPRITES) {
            return;
        }

        static char numberStr[16]{};
        const u32 strLength = ItoaSigned(pActor->effectState.value, numberStr);

        // Ascii character '*' = 0x2A
        constexpr u8 chrOffset = 0x2A;

        SpriteLayer& layer = spriteLayers[pActor->drawState.layer];

        const s32 frameCount = currentAnim.frameCount;
        const glm::i16vec2 pixelPos = WorldPosToScreenPixels(pActor->position);

        for (u32 c = 0; c < strLength; c++) {
            Sprite* outSprite = GetNextFreeSprite(&layer);
            if (outSprite == nullptr) {
                break;
            }

            const s32 frameIndex = (numberStr[c] - chrOffset) % frameCount;
            const u8 tileId = Metasprites::GetMetasprite(currentAnim.metaspriteIndex)->spritesRelativePos[frameIndex].tileId;

            *outSprite = {
                u16(pixelPos.y),
                u16(pixelPos.x + c * 5),
                tileId,
                1
            };
        }
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
        pActor->initialVelocity = pActor->velocity;

        pActor->drawState = ActorDrawState{};

        pActor->pUpdateFn = nullptr;
        pActor->pDrawFn = nullptr;

        switch (pPrototype->type) {
        case ACTOR_TYPE_PLAYER: {
            pActor->playerState.damageCounter = 0;
            pActor->playerState.sitCounter = 0;
            pActor->playerState.flags.aimMode = PLAYER_AIM_FWD;
            pActor->playerState.flags.doubleJumped = false;
            pActor->playerState.flags.sitState = PLAYER_STANDING;
            pActor->playerState.flags.slowFall = false;
            pActor->drawState.layer = SPRITE_LAYER_FG;
            pActor->pDrawFn = DrawPlayer;
            break;
        }
        case ACTOR_TYPE_NPC: {
            pActor->npcState.health = pPrototype->npcData.health;
            pActor->npcState.damageCounter = 0;
            pActor->drawState.layer = SPRITE_LAYER_FG;
            break;
        }
        case ACTOR_TYPE_BULLET: {
            pActor->bulletState.lifetime = pPrototype->bulletData.lifetime;
            pActor->bulletState.lifetimeCounter = pPrototype->bulletData.lifetime;
            pActor->drawState.layer = SPRITE_LAYER_FG;
            break;
        }
        case ACTOR_TYPE_PICKUP: {
            pActor->drawState.layer = SPRITE_LAYER_FG;
            break;
        }
        case ACTOR_TYPE_EFFECT: {
            pActor->effectState.lifetime = pPrototype->effectData.lifetime;
            pActor->effectState.lifetimeCounter = pPrototype->effectData.lifetime;
            pActor->drawState.layer = SPRITE_LAYER_FX;

            if (pPrototype->subtype == EFFECT_SUBTYPE_NUMBERS) {
                pActor->pDrawFn = DrawNumbers;
            }

            break;
        }
        case ACTOR_TYPE_CHECKPOINT: {
            const PersistedActorState* pPersistState = persistedActorStates.Get(pActor->id);
            if (pPersistState && pPersistState->activated) {
                pActor->checkpointState.activated = true;
            }
            pActor->drawState.layer = SPRITE_LAYER_BG;
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
        pActor->velocity = glm::vec2{};
        InitializeActor(pActor);

        return pActor;
    }

    static Actor* SpawnActor(u32 presetIndex, const glm::vec2& position, const glm::vec2& velocity = glm::vec2{}) {
        auto handle = actors.Add();

        if (handle == PoolHandle<Actor>::Null()) {
            return nullptr;
        }

        Actor* pActor = actors.Get(handle);
        // Clear previous data
        *pActor = Actor{};

        const ActorPrototype* pPrototype = Actors::GetPrototype(presetIndex);
        pActor->id = Random::GenerateUUID();
        pActor->pPrototype = pPrototype;
        if (pPrototype->type == ACTOR_TYPE_PLAYER) {
            playerHandle = handle;
        }

        pActor->position = position;
        pActor->velocity = velocity;
        InitializeActor(pActor);

        return pActor;
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
        if (!pActor->flags.active || pActor->flags.pendingRemoval) {
            return;
        }
        
        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pOther = actors.Get(handle);

            if (pOther == nullptr || pOther->pPrototype->type != type || pOther->pPrototype->alignment != alignment || pOther->flags.pendingRemoval || !pOther->flags.active) {
                continue;
            }

            if (ActorsColliding(pActor, pOther)) {
                callback(pActor, pOther);
            }
        }
    }

    static bool ActorCollidesWithPlayer(Actor* pActor, Actor* pPlayer) {
        if (pPlayer == nullptr || playerHealth == 0) {
            return false;
        }

        return ActorsColliding(pActor, pPlayer);
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

        Collision::SweepBoxHorizontal(pCurrentLevel->pTilemap, hitbox, pActor->position, dx, outHit);
        pActor->position.x = outHit.location.x;
        return outHit.blockingHit;
    }

    static bool ActorMoveVertical(Actor* pActor, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dy = pActor->velocity.y;

        Collision::SweepBoxVertical(pCurrentLevel->pTilemap, hitbox, pActor->position, dy, outHit);
        pActor->position.y = outHit.location.y;
        return outHit.blockingHit;
    }

    static void ApplyGravity(Actor* pActor, r32 gravity = 0.01f) {
        pActor->velocity.y += gravity;
    }
#pragma endregion

#pragma region Damage
    struct ExpAnimState {
        const u16& targetExp;
        u16& currentExp;

        r32 progress = 0.0f;
    };

    static bool AnimateExpCoroutine(void* userData) {
        ExpAnimState& state = *(ExpAnimState*)userData;

        state.progress += 0.05f;
        const r32 t = glm::smoothstep(0.0f, 1.0f, state.progress);
        state.currentExp = glm::mix(state.currentExp, state.targetExp, t);

        return state.currentExp != state.targetExp;
    }

    static void AnimatePlayerExp() {
        ExpAnimState state = {
                .targetExp = playerExp,
                .currentExp = playerDispExp,
        };
        StartCoroutine(AnimateExpCoroutine, state);
    }

    // TODO: Where to get this info properly?
    constexpr u16 largeExpValue = 500;
    constexpr u16 smallExpValue = 10;

    struct SpawnExpState {
        glm::vec2 position;
        u16 remainingValue;
    };

    static bool SpawnExpCoroutine(void* s) {
        SpawnExpState& state = *(SpawnExpState*)s;

        if (state.remainingValue > 0) {
            u16 spawnedValue = state.remainingValue >= largeExpValue ? largeExpValue : smallExpValue;
            s32 prototypeIndex = spawnedValue >= largeExpValue ? haloLargePrototypeIndex : haloSmallPrototypeIndex;

            const r32 speed = Random::GenerateReal(0.1f, 0.3f);
            const glm::vec2 velocity = Random::GenerateDirection() * speed;

            Actor* pSpawned = SpawnActor(prototypeIndex, state.position, velocity);

            pSpawned->pickupState.lingerCounter = 30;
            pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
            pSpawned->pickupState.value = pSpawned->pPrototype->pickupData.value;

            if (state.remainingValue < spawnedValue) {
                state.remainingValue = 0;
            }
            else state.remainingValue -= spawnedValue;

            return true;
        }
        return false;
    }

    static void NPCDie(Actor* pActor) {
        pActor->flags.pendingRemoval = true;

        PersistedActorState* persistState = persistedActorStates.Get(pActor->id);
        if (persistState) {
            persistState->dead = true;
        }
        else {
            persistedActorStates.Add(pActor->id, {
                .dead = true,
                });
        }

        Audio::PlaySFX(&enemyDieSfx, CHAN_ID_NOISE);
        SpawnActor(pActor->pPrototype->npcData.spawnOnDeath, pActor->position);

        // Spawn exp halos
        const u16 totalExpValue = pActor->pPrototype->npcData.expValue;
        if (totalExpValue > 0) {
            SpawnExpState coroutineState = {
                .position = pActor->position,
                .remainingValue = totalExpValue
            };
            StartCoroutine(SpawnExpCoroutine, coroutineState);
        }
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
            Random::GenerateReal(hitbox.x1, hitbox.x2),
            Random::GenerateReal(hitbox.y1, hitbox.y2)
        };
        const glm::vec2 spawnPos = pActor->position + randomPointInsideHitbox;

        constexpr glm::vec2 velocity = { 0, -0.03125f };
        Actor* pDmg = SpawnActor(dmgNumberPrototypeIndex, spawnPos, velocity);
        if (pDmg != nullptr) {
            pDmg->effectState.value = -dmgValue;
        }

        if (health <= 0) {
            return false;
        }

        return true;
    }
#pragma endregion

#pragma region Player logic
    static void CorrectPlayerSpawnY(const Level* pLevel, Actor* pPlayer) {
        HitResult hit{};

        const r32 dy = VIEWPORT_HEIGHT_METATILES / 2.0f;  // Sweep downwards to find a floor

        Collision::SweepBoxVertical(pLevel->pTilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
        while (hit.startPenetrating) {
            pPlayer->position.y -= 1.0f;
            Collision::SweepBoxVertical(pLevel->pTilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
        }
        pPlayer->position = hit.location;
    }

    static bool SpawnPlayerAtCheckpoint() {
        Actor* pCheckpoint = nullptr;

        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pActor = actors.Get(handle);

            if (pActor == nullptr) {
                continue;
            }

            if (pActor->pPrototype->type == ACTOR_TYPE_CHECKPOINT) {
                pCheckpoint = pActor;
                break;
            }
        }

        if (pCheckpoint == nullptr) {
            return false;
        }

        Actor* pPlayer = SpawnActor(playerPrototypeIndex, pCheckpoint->position);
        
        return true;
    }

    static void SpawnPlayerAtEntrance(const Level* pLevel, u8 screenIndex, u8 direction) {
        if (direction == SCREEN_EXIT_DIR_DEATH_WARP && SpawnPlayerAtCheckpoint()) {
            return;
        }

        r32 x = (screenIndex % TILEMAP_MAX_DIM_SCREENS) * VIEWPORT_WIDTH_METATILES;
        r32 y = (screenIndex / TILEMAP_MAX_DIM_SCREENS) * VIEWPORT_HEIGHT_METATILES;

        Actor* pPlayer = SpawnActor(playerPrototypeIndex, glm::vec2(x, y));
        if (pPlayer == nullptr) {
            return;
        }

        constexpr r32 initialHSpeed = 0.0625f;

        switch (direction) {
        case SCREEN_EXIT_DIR_LEFT: {
            pPlayer->position.x += 0.5f;
            pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
            pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
            pPlayer->velocity.x = initialHSpeed;
            CorrectPlayerSpawnY(pLevel, pPlayer);
            break;
        }
        case SCREEN_EXIT_DIR_RIGHT: {
            pPlayer->position.x += VIEWPORT_WIDTH_METATILES - 0.5f;
            pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
            pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
            pPlayer->velocity.x = -initialHSpeed;
            CorrectPlayerSpawnY(pLevel, pPlayer);
            break;
        }
        case SCREEN_EXIT_DIR_TOP: {
            pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
            pPlayer->position.y += 0.5f;
            pPlayer->velocity.y = 0.25f;
            break;
        }
        case SCREEN_EXIT_DIR_BOTTOM: {
            pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
            pPlayer->position.y += VIEWPORT_HEIGHT_METATILES - 0.5f;
            pPlayer->velocity.y = -0.25f;
            break;
        }
        default:
            break;
        }

        pPlayer->playerState.entryDelayCounter = 15;
    }

    static void UpdateFadeToBlack(r32 progress) {
        progress = glm::smoothstep(0.0f, 1.0f, progress);

        for (u32 i = 0; i < PALETTE_MEMORY_SIZE; i++) {
            u8 baseColor = ((u8*)basePaletteColors)[i];

            const s32 baseBrightness = (baseColor & 0b1110000) >> 4;
            const s32 hue = baseColor & 0b0001111;

            const s32 minBrightness = hue == 0 ? 0 : -1;

            s32 newBrightness = glm::mix(minBrightness, baseBrightness, 1.0f - progress);

            if (newBrightness >= 0) {
                ((u8*)pPalettes)[i] = hue | (newBrightness << 4);
            }
            else {
                ((u8*)pPalettes)[i] = 0x00;
            }
        }
    }

    enum LevelTransitionStatus : u8 {
        TRANSITION_FADE_OUT,
        TRANSITION_LOADING,
        TRANSITION_FADE_IN,
        TRANSITION_COMPLETE
    };

    struct LevelTransitionState {
        u16 nextLevelIndex;
        u16 nextScreenIndex;
        u8 nextDirection;
        
        r32 progress = 0.0f;
        u8 status = TRANSITION_FADE_OUT;
        u8 holdTimer = 12;
    };

    static bool LevelTransitionCoroutine(void* userData) {
        LevelTransitionState* state = (LevelTransitionState*)userData;

        switch (state->status) {
        case TRANSITION_FADE_OUT: {
            if (state->progress < 1.0f) {
                state->progress += 0.1f;
                UpdateFadeToBlack(state->progress);
                return true;
            }
            state->status = TRANSITION_LOADING;
            break;
        }
        case TRANSITION_LOADING: {
            if (state->holdTimer > 0) {
                state->holdTimer--;
                return true;
            }
            LoadLevel(state->nextLevelIndex, state->nextScreenIndex, state->nextDirection);
            state->status = TRANSITION_FADE_IN;
            pauseGameplay = false;
            break;
        }
        case TRANSITION_FADE_IN: {
            if (state->progress > 0.0f) {
                state->progress -= 0.10f;
                UpdateFadeToBlack(state->progress);
                return true;
            }
            state->status = TRANSITION_COMPLETE;
            break;
        }
        default:
            return false;
        }
    }

    static void HandleLevelExit() {
        const Actor* pPlayer = actors.Get(playerHandle);
        if (pPlayer == nullptr) {
            return;
        }

        bool shouldExit = false;
        u8 exitDirection = 0;
        u8 enterDirection = 0;
        u32 xScreen = 0;
        u32 yScreen = 0;

        // Left side of screen is ugly, so trigger transition earlier
        if (pPlayer->position.x < 0.5f) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_LEFT;
            enterDirection = SCREEN_EXIT_DIR_RIGHT;
            xScreen = 0;
            yScreen = glm::clamp(s32(pPlayer->position.y / VIEWPORT_HEIGHT_METATILES), 0, pCurrentLevel->pTilemap->height);
        }
        else if (pPlayer->position.x >= pCurrentLevel->pTilemap->width * VIEWPORT_WIDTH_METATILES) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_RIGHT;
            enterDirection = SCREEN_EXIT_DIR_LEFT;
            xScreen = pCurrentLevel->pTilemap->width - 1;
            yScreen = glm::clamp(s32(pPlayer->position.y / VIEWPORT_HEIGHT_METATILES), 0, pCurrentLevel->pTilemap->height);
        }
        else if (pPlayer->position.y < 0) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_TOP;
            enterDirection = SCREEN_EXIT_DIR_BOTTOM;
            xScreen = glm::clamp(s32(pPlayer->position.x / VIEWPORT_WIDTH_METATILES), 0, pCurrentLevel->pTilemap->width);
            yScreen = 0;
        }
        else if (pPlayer->position.y >= pCurrentLevel->pTilemap->height * VIEWPORT_HEIGHT_METATILES) {
            shouldExit = true;
            exitDirection = SCREEN_EXIT_DIR_BOTTOM;
            enterDirection = SCREEN_EXIT_DIR_TOP;
            xScreen = glm::clamp(s32(pPlayer->position.x / VIEWPORT_WIDTH_METATILES), 0, pCurrentLevel->pTilemap->width);
            yScreen = pCurrentLevel->pTilemap->height - 1;
        }

        if (shouldExit) {
            const u32 screenIndex = xScreen + TILEMAP_MAX_DIM_SCREENS * yScreen;
            const TilemapScreen& screen = pCurrentLevel->pTilemap->screens[screenIndex];
            const LevelExit* exits = (LevelExit*)&screen.screenMetadata;

            const LevelExit& exit = exits[exitDirection];

            LevelTransitionState state = {
                .nextLevelIndex = exit.targetLevel,
                .nextScreenIndex = exit.targetScreen,
                .nextDirection = enterDirection,
            };
            StartCoroutine(LevelTransitionCoroutine, state);
            pauseGameplay = true;
        }
    }

    struct HealthAnimState {
        const u16& targetHealth;
        u16& currentHealth;

        u16 delay = 12;
        r32 progress = 0.0f;
    };

    static bool AnimateHealthCoroutine(void* userData) {
        HealthAnimState& state = *(HealthAnimState*)userData;

        if (state.delay > 0) {
            state.delay--;
            return true;
        }

        state.progress += 0.01f;
        const r32 t = glm::smoothstep(0.0f, 1.0f, state.progress);
        state.currentHealth = glm::mix(state.currentHealth, state.targetHealth, t);

        return state.currentHealth != state.targetHealth;
    }

    static void AnimatePlayerHealth(u16 previousHealth) {
        playerDispYellowHealth = previousHealth;
        playerDispRedHealth = playerHealth;
        HealthAnimState state = {
            .targetHealth = playerHealth,
            .currentHealth = playerDispYellowHealth
        };
        StartCoroutine(AnimateHealthCoroutine, state);
    }

    struct ScreenShakeState {
        const s16 magnitude;
        u16 length;
    };

    static bool ShakeScreenCoroutine(void* userData) {
        ScreenShakeState& state = *(ScreenShakeState*)userData;

        if (state.length == 0) {
            pauseGameplay = false;
            return false;
        }

        const glm::vec2 viewportPos = GetViewportPos();
        const Scanline scanline = {
            (s32)(viewportPos.x * METATILE_DIM_PIXELS) + Random::GenerateInt(-state.magnitude, state.magnitude),
            (s32)(viewportPos.y * METATILE_DIM_PIXELS) + Random::GenerateInt(-state.magnitude, state.magnitude)
        };
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = scanline;
        }

        state.length--;
        return true;
    }

    static void PlayerRevive() {
        // TODO: Animate standing up

        // Restore life
        playerHealth = playerMaxHealth;
        AnimatePlayerHealth(0);
    }

    static void PlayerDie(Actor* pPlayer) {
        expRemnant = {
            .levelIndex = Levels::GetIndex(pCurrentLevel),
            .position = pPlayer->position,
            .value = playerExp
        };
        playerExp = 0;
        AnimatePlayerExp();

        // Transition to checkpoint
        LevelTransitionState state = {
                .nextLevelIndex = lastCheckpoint.levelIndex,
                .nextScreenIndex = lastCheckpoint.screenIndex,
                .nextDirection = SCREEN_EXIT_DIR_DEATH_WARP,
        };
        StartCoroutine(LevelTransitionCoroutine, state, PlayerRevive);
        pauseGameplay = true;
    }

    static void SpawnFeathers(Actor* pPlayer, u32 count) {
        for (u32 i = 0; i < count; i++) {
            const glm::vec2 spawnOffset = {
                Random::GenerateReal(-1.0f, 1.0f),
                Random::GenerateReal(-1.0f, 1.0f)
            };

            const glm::vec2 velocity = Random::GenerateDirection() * 0.0625f;
            Actor* pSpawned = SpawnActor(featherPrototypeIndex, pPlayer->position + spawnOffset, velocity);
            pSpawned->drawState.frameIndex = Random::GenerateInt(0, pSpawned->pPrototype->animations[0].frameCount - 1);
        }
    }

    static void PlayerMortalHit(Actor* pPlayer) {
        pauseGameplay = true;
        ScreenShakeState state = {
            .magnitude = 2,
            .length = 30
        };
        StartCoroutine(ShakeScreenCoroutine, state);
        pPlayer->velocity.y = -0.25f;
        playerDeathCounter = 240;
    }

    static void HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy) {
        // If invulnerable, or dead
        if (pPlayer->playerState.damageCounter != 0 || playerHealth == 0) {
            return;
        }

        const u32 damage = Random::GenerateInt(1, 20);
        Audio::PlaySFX(&damageSfx, CHAN_ID_PULSE0);

        u32 featherCount = Random::GenerateInt(1, 4);

        const u16 prevHealth = playerHealth;
        if (!ActorTakeDamage(pPlayer, damage, playerHealth, pPlayer->playerState.damageCounter)) {
            PlayerMortalHit(pPlayer);
            featherCount = 8;
        }

        SpawnFeathers(pPlayer, featherCount);
        AnimatePlayerHealth(prevHealth);

        // Recoil
        constexpr r32 recoilSpeed = 0.046875f; // Recoil speed from Zelda 2
        if (pEnemy->position.x > pPlayer->position.x) {
            pPlayer->flags.facingDir = 1;
            pPlayer->velocity.x = -recoilSpeed;
        }
        else {
            pPlayer->flags.facingDir = -1;
            pPlayer->velocity.x = recoilSpeed;
        }

    }

    static void PlayerSitDown(Actor* pPlayer) {
        pPlayer->playerState.flags.sitState = PLAYER_STAND_TO_SIT;
        pPlayer->playerState.sitCounter = 15;
    }

    static void PlayerStandUp(Actor* pPlayer) {
        pPlayer->playerState.flags.sitState = PLAYER_SIT_TO_STAND;
        pPlayer->playerState.sitCounter = 15;
    }

    static void TriggerInteraction(Actor* pPlayer) {
        if (interactableHandle == PoolHandle<Actor>::Null()) {
            return;
        }

        Actor* pInteractable = actors.Get(interactableHandle);
        if (pInteractable == nullptr) {
            return;
        }

        if (pInteractable->pPrototype->type == ACTOR_TYPE_CHECKPOINT) {
            pInteractable->checkpointState.activated = true;

            PersistedActorState* persistState = persistedActorStates.Get(pInteractable->id);
            if (persistState) {
                persistState->activated = true;
            }
            else {
                persistedActorStates.Add(pInteractable->id, {
                    .activated = true,
                    });
            }

            lastCheckpoint = {
                .levelIndex = u16(Levels::GetIndex(pCurrentLevel)),
                .screenIndex = GetScreenIndex(pInteractable->position)
            };

            // Revive enemies
            persistedActorStates.ForEach(ReviveKilledEnemy);

            // Sit down
            PlayerSitDown(pPlayer);

            // Add dialogue
            static constexpr const char* lines[] = {
                "I just put a regular dialogue box here, but it would\nnormally be a level up menu.",
            };

			if (!Game::IsDialogActive()) {
				Game::OpenDialog(lines, 1);
			}
        }
    }

    static void PlayerShoot(Actor* pPlayer) {
        constexpr s32 shootDelay = 10;

        PlayerState& playerState = pPlayer->playerState;
        UpdateCounter(playerState.shootCounter);

        if (Input::ButtonDown(BUTTON_B) && playerState.shootCounter == 0) {
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

    static void PlayerInput(Actor* pPlayer) {
        constexpr r32 maxSpeed = 0.09375f; // Actual movement speed from Zelda 2
        constexpr r32 acceleration = maxSpeed / 24.f; // Acceleration from Zelda 2

        const bool dead = playerHealth == 0;
        const bool enteringLevel = pPlayer->playerState.entryDelayCounter > 0;
        const bool stunned = pPlayer->playerState.damageCounter > 0;
        const bool sitting = pPlayer->playerState.flags.sitState != PLAYER_STANDING;

        const bool inputDisabled = dead || enteringLevel || stunned || sitting || Game::IsDialogActive();

        PlayerState& playerState = pPlayer->playerState;
        if (!inputDisabled && Input::ButtonDown(BUTTON_DPAD_LEFT)) {
            pPlayer->velocity.x -= acceleration;
            if (pPlayer->flags.facingDir != ACTOR_FACING_LEFT) {
                pPlayer->velocity.x -= acceleration;
            }

            pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
            pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
        }
        else if (!inputDisabled && Input::ButtonDown(BUTTON_DPAD_RIGHT)) {
            pPlayer->velocity.x += acceleration;
            if (pPlayer->flags.facingDir != ACTOR_FACING_RIGHT) {
                pPlayer->velocity.x += acceleration;
            }

            pPlayer->velocity.x = glm::clamp(pPlayer->velocity.x, -maxSpeed, maxSpeed);
            pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
        }
        else if (!enteringLevel && !pPlayer->flags.inAir && pPlayer->velocity.x != 0.0f) { // Decelerate
            pPlayer->velocity.x -= acceleration * glm::sign(pPlayer->velocity.x);
        }

        // Interaction / Shooting
        if (Game::IsDialogActive() && Input::ButtonPressed(BUTTON_B)) {
            Game::AdvanceDialogText();
        }
        else if (!inputDisabled) {
            if (interactableHandle != PoolHandle<Actor>::Null() && Input::ButtonPressed(BUTTON_B)) {
            TriggerInteraction(pPlayer);
            }
            else PlayerShoot(pPlayer);
        }

        if (inputDisabled) {
            if (!Game::IsDialogActive() && pPlayer->playerState.flags.sitState == PLAYER_SITTING && Input::AnyButtonDown()) {
                PlayerStandUp(pPlayer);
            }

            return;
        }

        // Aim mode
        if (Input::ButtonDown(BUTTON_DPAD_UP)) {
            playerState.flags.aimMode = PLAYER_AIM_UP;
        }
        else if (Input::ButtonDown(BUTTON_DPAD_DOWN)) {
            playerState.flags.aimMode = PLAYER_AIM_DOWN;
        }
        else playerState.flags.aimMode = PLAYER_AIM_FWD;

        if (Input::ButtonPressed(BUTTON_A) && (!pPlayer->flags.inAir || !playerState.flags.doubleJumped)) {
            pPlayer->velocity.y = -0.25f;
            if (pPlayer->flags.inAir) {
                playerState.flags.doubleJumped = true;
            }

            // Trigger new flap by taking wings out of falling position by advancing the frame index
            playerState.wingFrame = ++playerState.wingFrame % PLAYER_WING_FRAME_COUNT;

            Audio::PlaySFX(&jumpSfx, CHAN_ID_PULSE0);
        }

        if (pPlayer->velocity.y < 0 && Input::ButtonReleased(BUTTON_A)) {
            pPlayer->velocity.y /= 2;
        }

        if (Input::ButtonDown(BUTTON_A) && pPlayer->velocity.y > 0) {
            playerState.flags.slowFall = true;
        }

        if (Input::ButtonReleased(BUTTON_B)) {
            playerState.shootCounter = 0.0f;
        }

        if (Input::ButtonPressed(BUTTON_SELECT)) {
            if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
                playerWeapon = PLAYER_WEAPON_BOW;
            }
            else playerWeapon = PLAYER_WEAPON_LAUNCHER;
        }
    }

    static void AnimatePlayerDead(Actor* pPlayer) {
        u8 frameIdx = !pPlayer->flags.inAir;

        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerDeadBankOffsets[frameIdx],
            pChr[1].tiles + playerHeadFrameChrOffset,
            8
        );

        pPlayer->drawState.animIndex = 2;
        pPlayer->drawState.frameIndex = frameIdx;
		pPlayer->drawState.pixelOffset = { 0, 0 };
		pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
		pPlayer->drawState.useCustomPalette = false;
    }

    static void AnimatePlayerSitting(Actor* pPlayer) {
        u8 frameIdx = 1;

        // If in transition state
        if (pPlayer->playerState.flags.sitState & 0b10) {
            frameIdx = ((pPlayer->playerState.flags.sitState & 0b01) ^ (pPlayer->playerState.sitCounter >> 3)) & 1;
        }

        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerSitBankOffsets[frameIdx],
            pChr[1].tiles + playerHeadFrameChrOffset,
            8
        );

        // Get wings in sitting position
        const bool wingsInPosition = pPlayer->playerState.wingFrame == PLAYER_WINGS_FLAP_END;
        const u16 wingAnimFrameLength = 6;

        if (!wingsInPosition) {
            AdvanceAnimation(pPlayer->playerState.wingCounter, pPlayer->playerState.wingFrame, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength, 0);
        }

        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerWingFrameBankOffsets[pPlayer->playerState.wingFrame],
            pChr[1].tiles + playerWingFrameChrOffset,
            playerWingFrameTileCount
        );

        pPlayer->drawState.animIndex = 1;
        pPlayer->drawState.frameIndex = 0;
		pPlayer->drawState.pixelOffset = { 0, 0 };
		pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
		pPlayer->drawState.useCustomPalette = false;
    }

    static void AnimatePlayer(Actor* pPlayer) {
        PlayerState& playerState = pPlayer->playerState;

        if (playerHealth == 0) {
            AnimatePlayerDead(pPlayer);
            return;
        }

        if (playerState.flags.sitState != PLAYER_STANDING) {
            AnimatePlayerSitting(pPlayer);
            return;
        }

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

        pPlayer->drawState.animIndex = 0;
        pPlayer->drawState.frameIndex = playerState.flags.aimMode;
		pPlayer->drawState.pixelOffset = { 0, vOffset };
		pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
		SetDamagePaletteOverride(pPlayer, playerState.damageCounter);
    }

    static bool IsInteractable(const Actor* pActor) {
        if (pActor->pPrototype->type == ACTOR_TYPE_NPC && pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
            return true;
        }

        if (pActor->pPrototype->type == ACTOR_TYPE_CHECKPOINT) {
            return true;
        }

        return false;
    }

    static void FindPlayerInteractableActor(Actor* pPlayer) {
        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pOther = actors.Get(handle);

            if (pOther == nullptr || pOther->flags.pendingRemoval || !pOther->flags.active) {
                continue;
            }

            if (IsInteractable(pOther) && ActorsColliding(pPlayer, pOther)) {
                interactableHandle = handle;
                return;
            }
        }

        interactableHandle = PoolHandle<Actor>::Null();
    }

    static void UpdatePlayerSidescroller(Actor* pActor) {
        UpdateCounter(pActor->playerState.entryDelayCounter);
        UpdateCounter(pActor->playerState.damageCounter);

        if (!UpdateCounter(pActor->playerState.sitCounter)) {
            pActor->playerState.flags.sitState &= 1;
        }
        
        if (playerDeathCounter != 0) {
            if (!UpdateCounter(playerDeathCounter)) {
                PlayerDie(pActor);
            }
        }
        
        // Reset slow fall
        pActor->playerState.flags.slowFall = false;

        FindPlayerInteractableActor(pActor);

        PlayerInput(pActor);

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

        AnimatePlayer(pActor);
    }
#pragma endregion

#pragma region Bullets
    static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
        pBullet->flags.pendingRemoval = true;
        SpawnActor(pBullet->pPrototype->bulletData.spawnOnDeath, effectPos);
    }

    static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
        BulletDie(pBullet, pBullet->position);

        const u32 damage = Random::GenerateInt(1, 2);
        if (!ActorTakeDamage(pEnemy, damage, pEnemy->npcState.health, pEnemy->npcState.damageCounter)) {
            NPCDie(pEnemy);
        }
    }

    static void BulletCollision(Actor* pActor) {
        if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
            ForEachActorCollision(pActor, ACTOR_TYPE_NPC, ACTOR_ALIGNMENT_HOSTILE, HandleBulletEnemyCollision);
        }
        else if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE) {
            Actor* pPlayer = actors.Get(playerHandle);
            if (ActorCollidesWithPlayer(pActor, pPlayer)) {
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
    }

    static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
        velocity = glm::reflect(velocity, normal);
        //Audio::PlaySFX(&ricochetSfx, CHAN_ID_PULSE0);
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
    }
#pragma endregion

#pragma region NPC
    static void UpdateSlimeEnemy(Actor* pActor) {
        UpdateCounter(pActor->npcState.damageCounter);

        if (!pActor->flags.inAir) {
            const bool shouldJump = Random::GenerateInt(0, 127) == 0;
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

        Actor* pPlayer = actors.Get(playerHandle);
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

		pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
        SetDamagePaletteOverride(pActor, pActor->npcState.damageCounter);
    }

    static void UpdateSkullEnemy(Actor* pActor) {
        UpdateCounter(pActor->npcState.damageCounter);

        ActorFacePlayer(pActor);

        static const r32 amplitude = 4.0f;
        const r32 sineTime = glm::sin(gameplayFramesElapsed / 60.f);
        pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

        // Shoot fireballs
        const bool shouldFire = Random::GenerateInt(0, 127) == 0;
        if (shouldFire) {

            Actor* pPlayer = actors.Get(playerHandle);
            if (pPlayer != nullptr) {
                const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
                const glm::vec2 velocity = playerDir * 0.0625f;

                SpawnActor(enemyFireballPrototypeIndex, pActor->position, velocity);
            }
        }

        Actor* pPlayer = actors.Get(playerHandle);
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
        SetDamagePaletteOverride(pActor, pActor->npcState.damageCounter);
    }
#pragma endregion

#pragma region Pickups
    static void UpdateExpHalo(Actor* pActor) {
        Actor* pPlayer = actors.Get(playerHandle);

        const glm::vec2 playerVec = pPlayer->position - pActor->position;
        const glm::vec2 playerDir = glm::normalize(playerVec);
        const r32 playerDist = glm::length(playerVec);

        // Wait for a while before homing towards player
        if (!UpdateCounter(pActor->pickupState.lingerCounter)) {
            constexpr r32 trackingFactor = 0.1f; // Adjust to control homing strength

            glm::vec2 desiredVelocity = (playerVec * trackingFactor) + pPlayer->velocity;
            pActor->velocity = glm::mix(pActor->velocity, desiredVelocity, trackingFactor); // Smooth velocity transition

        }
        else {
            // Slow down after initial explosion
            r32 speed = glm::length(pActor->velocity);
            if (speed != 0) {
                const glm::vec2 dir = glm::normalize(pActor->velocity);

                constexpr r32 deceleration = 0.01f;
                speed = glm::clamp(speed - deceleration, 0.0f, 1.0f); // Is 1.0 a good max value?
                pActor->velocity = dir * speed;
            }
        }

        pActor->position += pActor->velocity;

        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            Audio::PlaySFX(&expSfx, CHAN_ID_PULSE0);
            pActor->flags.pendingRemoval = true;

            playerExp += pActor->pickupState.value;
            AnimatePlayerExp();

            return;
        }

        // Smoothstep animation when inside specified radius from player
        const Animation& currentAnim = pActor->pPrototype->animations[0];
        constexpr r32 animRadius = 4.0f;

        pActor->drawState.frameIndex = glm::floor((1.0f - glm::smoothstep(0.0f, animRadius, playerDist)) * currentAnim.frameCount);
		pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    }

    static void UpdateExpRemnant(Actor* pActor) {
        Actor* pPlayer = actors.Get(playerHandle);
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            Audio::PlaySFX(&expSfx, CHAN_ID_PULSE0);
            pActor->flags.pendingRemoval = true;

            expRemnant.levelIndex = -1;

            playerExp += pActor->pickupState.value;
            ExpAnimState state = {
                .targetExp = playerExp,
                .currentExp = playerDispExp,
            };
            StartCoroutine(AnimateExpCoroutine, state);

            return;
        }
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
    }

    static void UpdateNumbers(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        pActor->position.y += pActor->velocity.y;

        DrawNumbers(pActor);
    }

    static void UpdateFeather(Actor* pActor) {
        UpdateDefaultEffect(pActor);

        constexpr r32 maxFallSpeed = 0.03125f;
        ApplyGravity(pActor, 0.005f);
        if (pActor->velocity.y > maxFallSpeed) {
            pActor->velocity.y = maxFallSpeed;
        }

        constexpr r32 amplitude = 2.0f;
        constexpr r32 timeMultiplier = 1 / 30.f;
        const u16 time = pActor->effectState.lifetimeCounter - pActor->effectState.lifetime;
        const r32 sineTime = glm::sin(time * timeMultiplier);
        pActor->velocity.x = pActor->initialVelocity.x * sineTime;

        pActor->position += pActor->velocity;
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
        case PICKUP_SUBTYPE_HALO: {
            UpdateExpHalo(pActor);
            break;
        }
        case PICKUP_SUBTYPE_XP_REMNANT: {
            UpdateExpRemnant(pActor);
            break;
        }
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
        case EFFECT_SUBTYPE_FEATHER: {
            UpdateFeather(pActor);
            break;
        }
        default:
            break;
        }
    }

    static void UpdateCheckpoint(Actor* pActor) {
        if (pActor->checkpointState.activated) {
            pActor->drawState.animIndex = 1;
        }
        else {
            pActor->drawState.animIndex = 0;
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
            case ACTOR_TYPE_CHECKPOINT: {
                UpdateCheckpoint(pActor);
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

    static void DrawActors() {
		for (u32 i = 0; i < actors.Count(); i++)
		{
			Actor* pActor = actors.Get(actors.GetHandle(i));
			if (pActor == nullptr || !pActor->flags.active) {
				continue;
			}

			if (pActor->pDrawFn) {
				pActor->pDrawFn(pActor);
			}
			else {
				DrawActor(pActor);
			}
		}
    }

    static void Step() {
		Input::Update();

        if (!paused) {
            gameplayFramesElapsed++;

            StepCoroutines();

            if (!pauseGameplay) {
                UpdateActors();
                UpdateViewport();
                HandleLevelExit();
            }

            ClearSpriteLayers(spriteLayers);
            DrawActors();
            // Draw HUD
            DrawPlayerHealthBar();
            DrawExpCounter();
        }

        /*if (ButtonPressed(BUTTON_START)) {
            if (!musicPlaying) {
                Audio::PlayMusic(&bgm, true);
            }
            else {
                Audio::StopMusic();
            }
            musicPlaying = !musicPlaying;
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
    void LoadLevel(u32 index, s32 screenIndex, u8 direction, bool refresh) {
        if (index >= MAX_LEVEL_COUNT) {
            DEBUG_ERROR("Level count exceeded!");
        }

        pCurrentLevel = Levels::GetLevelsPtr() + index;

        ReloadLevel(screenIndex, direction, refresh);
    }

    void UnloadLevel(bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        SetViewportPos(glm::vec2(0.0f), false);

        // Clear actors
        actors.Clear();

        if (refresh) {
            RefreshViewport();
        }
    }

    void ReloadLevel(s32 screenIndex, u8 direction, bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        UnloadLevel(refresh);

        for (u32 i = 0; i < pCurrentLevel->actors.Count(); i++)
        {
            auto handle = pCurrentLevel->actors.GetHandle(i);
            const Actor* pActor = pCurrentLevel->actors.Get(handle);

            const PersistedActorState* pPersistState = persistedActorStates.Get(pActor->id);
            if (!pPersistState || !(pPersistState->dead || pPersistState->permaDead)) {
                SpawnActor(pActor);
            }

        }

        // Spawn player in sidescrolling level
        if (pCurrentLevel->flags.type == LEVEL_TYPE_SIDESCROLLER) {
            SpawnPlayerAtEntrance(pCurrentLevel, screenIndex, direction);
            UpdateViewport();
        }

        // Spawn xp remnant
        if (expRemnant.levelIndex == Levels::GetIndex(pCurrentLevel)) {
            Actor* pRemnant = SpawnActor(xpRemnantPrototypeIndex, expRemnant.position);
            pRemnant->pickupState.value = expRemnant.value;
        }

        gameplayFramesElapsed = 0;

        if (refresh) {
            RefreshViewport();
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
        Levels::LoadLevels("assets/levels.lev");
        Actors::LoadPrototypes("assets/actors.prt");

        // Initialize scanline state
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = { 0, 0 };
        }

        SetViewportPos(glm::vec2(0.0f), false);

        // TEMP SOUND STUFF
        jumpSfx = Audio::LoadSound("assets/jump.nsf");
        gunSfx = Audio::LoadSound("assets/gun1.nsf");
        ricochetSfx = Audio::LoadSound("assets/ricochet.nsf");
        damageSfx = Audio::LoadSound("assets/damage.nsf");
        expSfx = Audio::LoadSound("assets/exp.nsf");
        enemyDieSfx = Audio::LoadSound("assets/enemydie.nsf");
        //bgm = Audio::LoadSound("assets/music.nsf");

        // TODO: Level should load palettes and tileset?
        LoadLevel(0);
    }

    void Free() {
        Audio::StopMusic();

        Audio::FreeSound(&jumpSfx);
        Audio::FreeSound(&gunSfx);
        Audio::FreeSound(&ricochetSfx);
        Audio::FreeSound(&damageSfx);
        Audio::FreeSound(&expSfx);
        Audio::FreeSound(&enemyDieSfx);
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
        ClearSpriteLayers(spriteLayers);
        paused = p;
    }

    Level* GetCurrentLevel() {
        return pCurrentLevel;
    }
    DynamicActorPool* GetActors() {
        return &actors;
    }
#pragma endregion
}