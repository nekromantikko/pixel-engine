#include "game.h"
#include "system.h"
#include "game_input.h"
#include "game_state.h"
#include <cstring>
#include <cstdio>
#include "level.h"
#include "game_rendering.h"
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
#include "game_ui.h"
#include "actor_prototypes.h"

namespace Game {
    r64 secondsElapsed = 0.0f;
    u32 clockCounter = 0;

    // Rendering data
    RenderSettings* pRenderSettings;
    ChrSheet* pChr;
    Nametable* pNametables;
    Scanline* pScanlines;
    Palette* pPalettes;

    bool paused = false;

    Sound jumpSfx;
    Sound gunSfx;
    Sound ricochetSfx;
    Sound damageSfx;
    Sound expSfx;
    Sound enemyDieSfx;

    //Sound bgm;
    //bool musicPlaying = false;

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
        Actor* pPlayer = GetPlayer();
        if (!pPlayer) {
            return;
        }

        const glm::vec2 viewportPos = Rendering::GetViewportPos();
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

        Rendering::SetViewportPos(viewportPos + delta);
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
#pragma endregion

#pragma region UI
    

    static void SetDamagePaletteOverride(Actor* pActor, u16 damageCounter) {
		if (damageCounter > 0) {
			pActor->drawState.useCustomPalette = true;
			pActor->drawState.palette = (GetFramesElapsed() / 3) % 4;
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
        glm::i16vec2 drawPos = Rendering::WorldPosToScreenPixels(pPlayer->position) + drawState.pixelOffset;

        // Draw weapon first
        glm::i16vec2 weaponOffset;
        u8 weaponFrameBankOffset;
        u32 weaponMetaspriteIndex;
        const u16 playerWeapon = GetPlayerWeapon();
        switch (playerWeapon) {
        case PLAYER_WEAPON_BOW: {
            weaponOffset = playerBowOffsets[pPlayer->state.playerState.flags.aimMode];
            weaponFrameBankOffset = playerBowFrameBankOffsets[pPlayer->state.playerState.flags.aimMode];
            weaponMetaspriteIndex = pPlayer->state.playerState.flags.aimMode == PLAYER_AIM_FWD ? playerBowFwdMetaspriteIndex : playerBowDiagMetaspriteIndex;
            break;
        }
        case PLAYER_WEAPON_LAUNCHER: {
            weaponOffset = playerLauncherOffsets[pPlayer->state.playerState.flags.aimMode];
            weaponFrameBankOffset = playerLauncherFrameBankOffsets[pPlayer->state.playerState.flags.aimMode];
            weaponMetaspriteIndex = pPlayer->state.playerState.flags.aimMode == PLAYER_AIM_FWD ? playerLauncherFwdMetaspriteIndex : playerLauncherDiagMetaspriteIndex;
            break;
        }
        default:
            break;
        }
        weaponOffset.x *= pPlayer->flags.facingDir;

		Rendering::CopyBankTiles(PLAYER_BANK_INDEX, weaponFrameBankOffset, 1, playerWeaponFrameChrOffset, playerWeaponFrameTileCount);
		Rendering::DrawMetasprite(SPRITE_LAYER_FG, weaponMetaspriteIndex, drawPos + weaponOffset, drawState.hFlip, drawState.vFlip, -1);

        return true;
    }

    static void DrawPlayer(Actor* pPlayer) {
        if (GetPlayerHealth() != 0 && pPlayer->state.playerState.flags.sitState == PLAYER_STANDING) {
            DrawPlayerGun(pPlayer);
        }
        DrawActorDefault(pPlayer);
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
        const u32 strLength = ItoaSigned(pActor->state.effectState.value, numberStr);

        // Ascii character '*' = 0x2A
        constexpr u8 chrOffset = 0x2A;

        const s32 frameCount = currentAnim.frameCount;
        const glm::i16vec2 pixelPos = Rendering::WorldPosToScreenPixels(pActor->position);

        for (u32 c = 0; c < strLength; c++) {
			glm::i16vec2 drawPos = pixelPos + glm::i16vec2{ c * 5, 0 };
            const s32 frameIndex = (numberStr[c] - chrOffset) % frameCount;

			Rendering::DrawMetaspriteSprite(pActor->drawState.layer, currentAnim.metaspriteIndex, frameIndex, drawPos, false, false, -1);
        }
    }
#pragma endregion

#pragma region Collision
    
    static bool ActorCollidesWithPlayer(Actor* pActor, Actor* pPlayer) {
        if (pPlayer == nullptr || GetPlayerHealth() == 0) {
            return false;
        }

        return ActorsColliding(pActor, pPlayer);
    }
#pragma endregion

#pragma region Movement
    static void ActorFacePlayer(Actor* pActor) {
        pActor->flags.facingDir = ACTOR_FACING_RIGHT;

        Actor* pPlayer = GetPlayer();
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

        Collision::SweepBoxHorizontal(GetCurrentLevel()->pTilemap, hitbox, pActor->position, dx, outHit);
        pActor->position.x = outHit.location.x;
        return outHit.blockingHit;
    }

    static bool ActorMoveVertical(Actor* pActor, HitResult& outHit) {
        const AABB& hitbox = pActor->pPrototype->hitbox;

        const r32 dy = pActor->velocity.y;

        Collision::SweepBoxVertical(GetCurrentLevel()->pTilemap, hitbox, pActor->position, dy, outHit);
        pActor->position.y = outHit.location.y;
        return outHit.blockingHit;
    }

    static void ApplyGravity(Actor* pActor, r32 gravity = 0.01f) {
        pActor->velocity.y += gravity;
    }
#pragma endregion

#pragma region Damage
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

            pSpawned->state.pickupState.lingerCounter = 30;
            pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
            pSpawned->state.pickupState.value = pSpawned->pPrototype->data.pickupData.value;

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

        PersistedActorData persistData = GetPersistedActorData(pActor->id);
        persistData.dead = true;
        SetPersistedActorData(pActor->id, persistData);

        Audio::PlaySFX(&enemyDieSfx, CHAN_ID_NOISE);
        SpawnActor(pActor->pPrototype->data.npcData.spawnOnDeath, pActor->position);

        // Spawn exp halos
        const u16 totalExpValue = pActor->pPrototype->data.npcData.expValue;
        if (totalExpValue > 0) {
            SpawnExpState coroutineState = {
                .position = pActor->position,
                .remainingValue = totalExpValue
            };
            StartCoroutine(SpawnExpCoroutine, coroutineState);
        }
    }

    static u16 ActorTakeDamage(Actor* pActor, u32 dmgValue, u16 currentHealth, u16& damageCounter) {
        constexpr s32 damageDelay = 30;

        u16 newHealth = currentHealth;
        if (dmgValue > newHealth) {
            newHealth = 0;
        }
        else newHealth -= dmgValue;
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
            pDmg->state.effectState.value = -dmgValue;
        }

        return newHealth;
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

    static bool ActorIsCheckpoint(const Actor* pActor) {
		return pActor->pPrototype->type == ACTOR_TYPE_CHECKPOINT;
    }

    static bool SpawnPlayerAtCheckpoint() {
        Actor* pCheckpoint = GetFirstActor(ActorIsCheckpoint);
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

        pPlayer->state.playerState.entryDelayCounter = 15;
    }

    static void UpdateFadeToBlack(r32 progress) {
        progress = glm::smoothstep(0.0f, 1.0f, progress);

        u8 colors[PALETTE_COLOR_COUNT];
        for (u32 i = 0; i < PALETTE_COUNT; i++) {
            Rendering::GetPalettePresetColors(i, colors);
			for (u32 j = 0; j < PALETTE_COLOR_COUNT; j++) {
                u8 color = colors[j];

                const s32 baseBrightness = (color & 0b1110000) >> 4;
                const s32 hue = color & 0b0001111;

                const s32 minBrightness = hue == 0 ? 0 : -1;

                s32 newBrightness = glm::mix(minBrightness, baseBrightness, 1.0f - progress);

                if (newBrightness >= 0) {
                    colors[j] = hue | (newBrightness << 4);
                }
                else {
                    colors[j] = 0x00;
                }
			}
            Rendering::WritePaletteColors(i, colors);
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
            UnfreezeGameplay();
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
        const Actor* pPlayer = GetPlayer();
        if (pPlayer == nullptr) {
            return;
        }

		Level* pCurrentLevel = GetCurrentLevel();
		if (pCurrentLevel == nullptr) {
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
            FreezeGameplay();
        }
    }

    struct ScreenShakeState {
        const s16 magnitude;
        u16 length;
    };

    static bool ShakeScreenCoroutine(void* userData) {
        ScreenShakeState& state = *(ScreenShakeState*)userData;

        if (state.length == 0) {
            UnfreezeGameplay();
            return false;
        }

        const glm::vec2 viewportPos = Rendering::GetViewportPos();
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
        SetPlayerHealth(GetPlayerMaxHealth());
    }

    static void PlayerDie(Actor* pPlayer) {
		Level* pCurrentLevel = GetCurrentLevel();

        SetExpRemnant(Levels::GetIndex(pCurrentLevel), pPlayer->position, GetPlayerExp());
		SetPlayerExp(0);

        // Transition to checkpoint
        const Checkpoint checkpoint = GetCheckpoint();
        LevelTransitionState state = {
                .nextLevelIndex = checkpoint.levelIndex,
                .nextScreenIndex = checkpoint.screenIndex,
                .nextDirection = SCREEN_EXIT_DIR_DEATH_WARP,
        };
        StartCoroutine(LevelTransitionCoroutine, state, PlayerRevive);
        FreezeGameplay();
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
        FreezeGameplay();
        ScreenShakeState state = {
            .magnitude = 2,
            .length = 30
        };
        StartCoroutine(ShakeScreenCoroutine, state);
        pPlayer->velocity.y = -0.25f;
        pPlayer->state.playerState.deathCounter = 240;
    }

    static void HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy) {
        u16 health = GetPlayerHealth();

        // If invulnerable, or dead
        if (pPlayer->state.playerState.damageCounter != 0 || health == 0) {
            return;
        }

        const u32 damage = Random::GenerateInt(1, 20);
        Audio::PlaySFX(&damageSfx, CHAN_ID_PULSE0);

        u32 featherCount = Random::GenerateInt(1, 4);

        health = ActorTakeDamage(pPlayer, damage, health, pPlayer->state.playerState.damageCounter);
        if (health == 0) {
            PlayerMortalHit(pPlayer);
            featherCount = 8;
        }

        SpawnFeathers(pPlayer, featherCount);
        SetPlayerHealth(health);

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
        pPlayer->state.playerState.flags.sitState = PLAYER_STAND_TO_SIT;
        pPlayer->state.playerState.sitCounter = 15;
    }

    static void PlayerStandUp(Actor* pPlayer) {
        pPlayer->state.playerState.flags.sitState = PLAYER_SIT_TO_STAND;
        pPlayer->state.playerState.sitCounter = 15;
    }

    static void TriggerInteraction(Actor* pPlayer, Actor* pInteractable) {
        if (pInteractable == nullptr) {
            return;
        }

        if (pInteractable->pPrototype->type == ACTOR_TYPE_CHECKPOINT) {
            pInteractable->state.checkpointState.activated = true;

            ActivateCheckpoint(pInteractable);

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

        PlayerState& playerState = pPlayer->state.playerState;
        UpdateCounter(playerState.shootCounter);

        if (Input::ButtonDown(BUTTON_B) && playerState.shootCounter == 0) {
            playerState.shootCounter = shootDelay;

            const u16 playerWeapon = GetPlayerWeapon();
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

    static bool IsInteractable(const Actor* pActor) {
        if (pActor->pPrototype->type == ACTOR_TYPE_NPC && pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
            return true;
        }

        if (pActor->pPrototype->type == ACTOR_TYPE_CHECKPOINT) {
            return true;
        }

        return false;
    }

    static void PlayerInput(Actor* pPlayer) {
        constexpr r32 maxSpeed = 0.09375f; // Actual movement speed from Zelda 2
        constexpr r32 acceleration = maxSpeed / 24.f; // Acceleration from Zelda 2

        const bool dead = GetPlayerHealth() == 0;
        const bool enteringLevel = pPlayer->state.playerState.entryDelayCounter > 0;
        const bool stunned = pPlayer->state.playerState.damageCounter > 0;
        const bool sitting = pPlayer->state.playerState.flags.sitState != PLAYER_STANDING;

        const bool inputDisabled = dead || enteringLevel || stunned || sitting || Game::IsDialogActive();

        PlayerState& playerState = pPlayer->state.playerState;
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
			Actor* pInteractable = GetFirstActorCollision(pPlayer, IsInteractable);
            if (pInteractable && Input::ButtonPressed(BUTTON_B)) {
            TriggerInteraction(pPlayer, pInteractable);
            }
            else PlayerShoot(pPlayer);
        }

        if (inputDisabled) {
            if (!Game::IsDialogActive() && pPlayer->state.playerState.flags.sitState == PLAYER_SITTING && Input::AnyButtonDown()) {
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
            u16 playerWeapon = GetPlayerWeapon();
            if (playerWeapon == PLAYER_WEAPON_LAUNCHER) {
                SetPlayerWeapon(PLAYER_WEAPON_BOW);
            }
            else SetPlayerWeapon(PLAYER_WEAPON_LAUNCHER);
        }
    }

    static void AnimatePlayerDead(Actor* pPlayer) {
        u8 frameIdx = !pPlayer->flags.inAir;

        Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerDeadBankOffsets[frameIdx], 1, playerHeadFrameChrOffset, 8);

        pPlayer->drawState.animIndex = 2;
        pPlayer->drawState.frameIndex = frameIdx;
		pPlayer->drawState.pixelOffset = { 0, 0 };
		pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
		pPlayer->drawState.useCustomPalette = false;
    }

    static void AnimatePlayerSitting(Actor* pPlayer) {
        u8 frameIdx = 1;

        // If in transition state
        if (pPlayer->state.playerState.flags.sitState & 0b10) {
            frameIdx = ((pPlayer->state.playerState.flags.sitState & 0b01) ^ (pPlayer->state.playerState.sitCounter >> 3)) & 1;
        }

		Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerSitBankOffsets[frameIdx], 1, playerHeadFrameChrOffset, 8);

        // Get wings in sitting position
        const bool wingsInPosition = pPlayer->state.playerState.wingFrame == PLAYER_WINGS_FLAP_END;
        const u16 wingAnimFrameLength = 6;

        if (!wingsInPosition) {
            AdvanceAnimation(pPlayer->state.playerState.wingCounter, pPlayer->state.playerState.wingFrame, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength, 0);
        }

        Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerWingFrameBankOffsets[pPlayer->state.playerState.wingFrame], 1, playerWingFrameChrOffset, playerWingFrameTileCount);

        pPlayer->drawState.animIndex = 1;
        pPlayer->drawState.frameIndex = 0;
		pPlayer->drawState.pixelOffset = { 0, 0 };
		pPlayer->drawState.hFlip = pPlayer->flags.facingDir == ACTOR_FACING_LEFT;
		pPlayer->drawState.useCustomPalette = false;
    }

    static void AnimatePlayer(Actor* pPlayer) {
        PlayerState& playerState = pPlayer->state.playerState;

        if (GetPlayerHealth() == 0) {
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
		Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerHeadFrameBankOffsets[playerState.flags.aimMode * 4 + headFrameIndex], 1, playerHeadFrameChrOffset, playerHeadFrameTileCount);

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
		Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerLegsFrameBankOffsets[legsFrameIndex], 1, playerLegsFrameChrOffset, playerLegsFrameTileCount);

        // When jumping or falling, wings get into proper position and stay there for the duration of the jump/fall
        const bool wingsInPosition = (jumping && playerState.wingFrame == PLAYER_WINGS_ASCEND) || (falling && playerState.wingFrame == PLAYER_WINGS_DESCEND);

        // Wings flap faster to get into proper position
        const u16 wingAnimFrameLength = (jumping || falling) ? 6 : 12;

        if (!wingsInPosition) {
            AdvanceAnimation(playerState.wingCounter, playerState.wingFrame, PLAYER_WING_FRAME_COUNT, wingAnimFrameLength, 0);
        }

		Rendering::CopyBankTiles(PLAYER_BANK_INDEX, playerWingFrameBankOffsets[playerState.wingFrame], 1, playerWingFrameChrOffset, playerWingFrameTileCount);

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

    static void UpdatePlayerSidescroller(Actor* pActor) {
        UpdateCounter(pActor->state.playerState.entryDelayCounter);
        UpdateCounter(pActor->state.playerState.damageCounter);

        if (!UpdateCounter(pActor->state.playerState.sitCounter)) {
            pActor->state.playerState.flags.sitState &= 1;
        }
        
        if (pActor->state.playerState.deathCounter != 0) {
            if (!UpdateCounter(pActor->state.playerState.deathCounter)) {
                PlayerDie(pActor);
            }
        }
        
        // Reset slow fall
        pActor->state.playerState.flags.slowFall = false;

        PlayerInput(pActor);

        HitResult hit{};
        if (ActorMoveHorizontal(pActor, hit)) {
            pActor->velocity.x = 0.0f;
        }

        constexpr r32 playerGravity = 0.01f;
        constexpr r32 playerSlowGravity = playerGravity / 4;

        const r32 gravity = pActor->state.playerState.flags.slowFall ? playerSlowGravity : playerGravity;
        ApplyGravity(pActor, gravity);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (ActorMoveVertical(pActor, hit)) {
            pActor->velocity.y = 0.0f;

            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
                pActor->state.playerState.flags.doubleJumped = false;
            }
        }

        AnimatePlayer(pActor);
    }
#pragma endregion

#pragma region Bullets
    static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
        pBullet->flags.pendingRemoval = true;
        SpawnActor(pBullet->pPrototype->data.bulletData.spawnOnDeath, effectPos);
    }

    static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
        BulletDie(pBullet, pBullet->position);

        const u32 damage = Random::GenerateInt(1, 2);
        const u16 newHealth = ActorTakeDamage(pEnemy, damage, pEnemy->state.npcState.health, pEnemy->state.npcState.damageCounter);
        if (newHealth == 0) {
            NPCDie(pEnemy);
        }
        pEnemy->state.npcState.health = newHealth;
    }

	static bool ActorIsHostileNPC(const Actor* pActor) {
		return pActor->pPrototype->type == ACTOR_TYPE_NPC && pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE;
	}

    static void BulletCollision(Actor* pActor) {
        if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_FRIENDLY) {
            ForEachActorCollision(pActor, HandleBulletEnemyCollision, ActorIsHostileNPC);
        }
        else if (pActor->pPrototype->alignment == ACTOR_ALIGNMENT_HOSTILE) {
            Actor* pPlayer = GetPlayer();
            if (ActorCollidesWithPlayer(pActor, pPlayer)) {
                HandlePlayerEnemyCollision(pPlayer, pActor);
                BulletDie(pActor, pActor->position);
            }
            // TODO: Collision with friendly NPC:s? Does this happen in the game?
        }
    }

    static void UpdateDefaultBullet(Actor* pActor) {
        if (!UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
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
        if (!UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
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
        if (!UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
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
        UpdateCounter(pActor->state.npcState.damageCounter);

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

        Actor* pPlayer = GetPlayer();
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

		pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
        SetDamagePaletteOverride(pActor, pActor->state.npcState.damageCounter);
    }

    static void UpdateSkullEnemy(Actor* pActor) {
        UpdateCounter(pActor->state.npcState.damageCounter);

        ActorFacePlayer(pActor);

        static const r32 amplitude = 4.0f;
        const r32 sineTime = glm::sin(GetFramesElapsed() / 60.f);
        pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

        // Shoot fireballs
        const bool shouldFire = Random::GenerateInt(0, 127) == 0;
        if (shouldFire) {

            Actor* pPlayer = GetPlayer();
            if (pPlayer != nullptr) {
                const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
                const glm::vec2 velocity = playerDir * 0.0625f;

                SpawnActor(enemyFireballPrototypeIndex, pActor->position, velocity);
            }
        }

        Actor* pPlayer = GetPlayer();
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            HandlePlayerEnemyCollision(pPlayer, pActor);
        }

        pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
        SetDamagePaletteOverride(pActor, pActor->state.npcState.damageCounter);
    }
#pragma endregion

#pragma region Pickups
    static void UpdateExpHalo(Actor* pActor) {
        Actor* pPlayer = GetPlayer();

        const glm::vec2 playerVec = pPlayer->position - pActor->position;
        const glm::vec2 playerDir = glm::normalize(playerVec);
        const r32 playerDist = glm::length(playerVec);

        // Wait for a while before homing towards player
        if (!UpdateCounter(pActor->state.pickupState.lingerCounter)) {
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

            AddPlayerExp(pActor->state.pickupState.value);

            return;
        }

        // Smoothstep animation when inside specified radius from player
        const Animation& currentAnim = pActor->pPrototype->animations[0];
        constexpr r32 animRadius = 4.0f;

        pActor->drawState.frameIndex = glm::floor((1.0f - glm::smoothstep(0.0f, animRadius, playerDist)) * currentAnim.frameCount);
		pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    }

    static void UpdateExpRemnant(Actor* pActor) {
        Actor* pPlayer = GetPlayer();
        if (ActorCollidesWithPlayer(pActor, pPlayer)) {
            Audio::PlaySFX(&expSfx, CHAN_ID_PULSE0);
            pActor->flags.pendingRemoval = true;

            ClearExpRemnant();
			AddPlayerExp(pActor->state.pickupState.value);

            return;
        }
    }
#pragma endregion

#pragma region Effects
    static void UpdateDefaultEffect(Actor* pActor) {
        if (!UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
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
        const u16 time = pActor->state.effectState.lifetimeCounter - pActor->state.effectState.lifetime;
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

        HandleLevelExit();
        UpdateViewport();
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
        if (pActor->state.checkpointState.activated) {
            pActor->drawState.animIndex = 1;
        }
        else {
            pActor->drawState.animIndex = 0;
        }
    }

    static void TempActorUpdateCallback(Actor* pActor) {
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

    static void Step() {
        if (!paused) {
            StepFrame(TempActorUpdateCallback);
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
    void Initialize() {
        // Rendering data
        pRenderSettings = ::Rendering::GetSettingsPtr();
        pChr = ::Rendering::GetChrPtr(0);
        pPalettes = ::Rendering::GetPalettePtr(0);
        pNametables = ::Rendering::GetNametablePtr(0);
        pScanlines = ::Rendering::GetScanlinePtr(0);

        Rendering::Init();

        Tiles::LoadTileset("assets/forest.til");
        Metasprites::Load("assets/meta.spr");
        Levels::LoadLevels("assets/levels.lev");
        Assets::LoadActorPrototypes("assets/actors.prt");

        // TEMP SOUND STUFF
        jumpSfx = Audio::LoadSound("assets/jump.nsf");
        gunSfx = Audio::LoadSound("assets/gun1.nsf");
        ricochetSfx = Audio::LoadSound("assets/ricochet.nsf");
        damageSfx = Audio::LoadSound("assets/damage.nsf");
        expSfx = Audio::LoadSound("assets/exp.nsf");
        enemyDieSfx = Audio::LoadSound("assets/enemydie.nsf");
        //bgm = Audio::LoadSound("assets/music.nsf");

		InitGameData();
        InitGameState(GAME_STATE_PLAYING);

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
        Rendering::ClearSpriteLayers();
        paused = p;
    }
#pragma endregion
}