#include "game.h"
#include "system.h"
#include "input.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "rendering_util.h"
#include "level.h"
#include "viewport.h"
#include "collision.h"
#include "metasprite.h"
#include "tileset.h"
#include "memory_pool.h"
#include <imgui.h>
#include "math.h"

static constexpr u32 viewportWidthInMetatiles = VIEWPORT_WIDTH_TILES / Tileset::metatileWorldSize;
static constexpr u32 viewportHeightInMetatiles = VIEWPORT_HEIGHT_TILES / Tileset::metatileWorldSize;

namespace Game {
    r64 secondsElapsed = 0.0f;
    r64 averageFramerate;
    // Number of successive frame times used for average framerate calculation
#define SUCCESSIVE_FRAME_TIME_COUNT 64
    r64 successiveFrameTimes[SUCCESSIVE_FRAME_TIME_COUNT]{0};

    // Seconds elapsed while not paused
    r64 gameplaySecondsElapsed = 0.0f;

    // Settings
    Rendering::Settings* pRenderSettings;

    // Viewport
    Viewport viewport;

    Level::Level* pCurrentLevel = nullptr;
    s32 enterScreenIndex = -1;

    // This is pretty ugly...
    u32 nextLevel = 0;
    s32 nextLevelScreenIndex = -1;

#define HUD_TILE_COUNT 128

    enum GameState {
        StatePlaying,
        StateLevelTransition,
    };

    GameState state = StatePlaying;

    struct LevelTransitionState {
        // Coordinates in metatiles
        IVec2 origin;
        IVec2 windowWorldPos;
        IVec2 windowSize;

        u32 steps;
        u32 currentStep;
        r32 stepDuration;

        r32 accumulator;
        bool direction;
    };

    LevelTransitionState levelTransitionState;

    // Sprite stufff
    enum HeadMode {
        HeadIdle,
        HeadFwd,
        HeadFall
    };

    enum LegsMode {
        LegsIdle,
        LegsFwd,
        LegsJump,
        LegsFall
    };

    enum WingMode {
        WingFlap,
        WingJump,
        WingFall
    };

    enum AimMode {
        AimFwd,
        AimUp,
        AimDown
    };

    enum Direction {
        DirLeft = -1,
        DirRight = 1
    };

    enum WeaponType {
        WpnBow,
        WpnLauncher
    };

    struct PlayerState {
        r32 x, y;
        r32 hSpeed, vSpeed;
        Direction direction;
        WeaponType weapon;
        HeadMode hMode;
        LegsMode lMode;
        WingMode wMode;
        AimMode aMode;
        r32 wingCounter;
        u32 wingFrame;
        s32 vOffset;
        bool slowFall;
        bool inAir;
        bool doubleJumped;
        r32 damageTimer;
    };

    PlayerState playerState{};
    r32 gravity = 70;

    struct Arrow {
        Vec2 pos;
        Vec2 vel;
        WeaponType type;
        u32 bounces;
    };

    Pool<Arrow> arrowPool;

    constexpr r32 shootDelay = 0.16f;
    r32 shootTimer = 0;

    constexpr u32 impactFrameCount = 4;
    constexpr r32 impactAnimLength = 0.16f;

    struct Impact {
        Vec2 pos;
        r32 accumulator;
    };
    Pool<Impact> hitPool;

    Vec2 enemyPos = Vec2{ 48.0f, 27.0f };

    constexpr r32 damageNumberLifetime = 1.0f;

    struct DamageNumber {
        Vec2 pos;
        s32 damage;
        r32 accumulator;
    };
    Pool<DamageNumber> damageNumberPool;


    struct Enemy {
        Vec2 pos;
        r32 spawnHeight;
        s32 health;
        r32 damageTimer;
    };
    Pool<Enemy> enemyPool;

    Rendering::ChrSheet playerBank;

    bool paused = false;

    constexpr r32 damageDelay = 0.5f;

    Rendering::Sprite* pSprites;
    Rendering::ChrSheet* pChr;
    Rendering::Nametable* pNametables;

    void LoadLevel(u32 index, s32 screenIndex, bool refresh) {
        if (index >= Level::maxLevelCount) {
            DEBUG_ERROR("Level count exceeded!");
        }

        pCurrentLevel = Level::GetLevelsPtr() + index;
        enterScreenIndex = screenIndex;

        ReloadLevel(refresh);
    }

    static bool SpawnAtFirstDoor(u32 screenIndex) {
        const Level::Screen& screen = pCurrentLevel->screens[screenIndex];
        for (u32 i = 0; i < Level::screenWidthMetatiles * Level::screenHeightMetatiles; i++) {
            const Level::LevelTile& tile = screen.tiles[i];

            if (tile.actorType == Level::ACTOR_DOOR) {
                const Vec2 screenRelativePos = Level::TileIndexToScreenOffset(i);
                const Vec2 worldPos = Level::ScreenOffsetToWorld(pCurrentLevel, screenRelativePos, screenIndex);

                playerState.x = worldPos.x + 1.0f;
                playerState.y = worldPos.y;

                return true;
            }
        }

        return false;
    }

    void ReloadLevel(bool refresh) {
        if (pCurrentLevel == nullptr) {
            return;
        }

        if (enterScreenIndex < 0 || !SpawnAtFirstDoor(enterScreenIndex)) {
            // Loop thru all screens to find any spawnpoint
            for (u32 i = 0; i < pCurrentLevel->screenCount; i++) {
                if (SpawnAtFirstDoor(i)) {
                    break;
                }
            }
        }

        enemyPool.Clear();
        // Spawn actors very stupidly
        for (u32 i = 0; i < pCurrentLevel->screenCount; i++) {
            const Level::Screen& screen = pCurrentLevel->screens[i];

            for (u32 t = 0; t < Level::screenWidthMetatiles * Level::screenHeightMetatiles; t++) {
                const Level::LevelTile& tile = screen.tiles[t];

                if (tile.actorType == Level::ACTOR_SKULL_ENEMY) {
                    const Vec2 screenRelativePos = Level::TileIndexToScreenOffset(t);
                    const Vec2 worldPos = Level::ScreenOffsetToWorld(pCurrentLevel, screenRelativePos, i);

                    PoolHandle<Enemy> handle = enemyPool.Add();
                    Enemy* enemy = enemyPool[handle];

                    enemy->pos = worldPos;
                    enemy->spawnHeight = worldPos.y;
                    enemy->health = 10;
                    enemy->damageTimer = 0.0f;
                }
            }
        }

        if (refresh) {
            RefreshViewport(&viewport, pNametables, pCurrentLevel);
        }
    }

	void Initialize(Rendering::RenderContext* pRenderContext) {
        viewport.x = 0.0f;
        viewport.y = 0.0f;

        playerState.x = 0;
        playerState.y = 0;
        playerState.direction = DirRight;
        playerState.weapon = WpnLauncher;

        // Init chr memory
        pChr = Rendering::GetChrPtr(pRenderContext, 0);
        // TODO: Pre-process these instead of loading from bitmap at runtime!
        Rendering::ChrSheet temp;
        Rendering::Util::CreateChrSheet("assets/chr000.bmp", &temp);
        Rendering::Util::CopyChrTiles(temp.tiles, pChr[0].tiles, 256);
        Rendering::Util::CreateChrSheet("assets/chr001.bmp", &temp);
        Rendering::Util::CopyChrTiles(temp.tiles, pChr[1].tiles, 256);

        Rendering::Util::CreateChrSheet("assets/player.bmp", &playerBank);

        u8 paletteColors[8 * 8];
        Rendering::Util::LoadPaletteColorsFromFile("assets/palette.dat", paletteColors);
        Rendering::Palette* palette = Rendering::GetPalettePtr(pRenderContext, 0);
        // This is kind of silly
        for (u32 i = 0; i < 8; i++) {
            memcpy(palette[i].colors, paletteColors + i * 8, 8);
        }

        pSprites = Rendering::GetSpritesPtr(pRenderContext, 0);
        Rendering::Util::ClearSprites(pSprites, MAX_SPRITE_COUNT);

        arrowPool.Init(512);
        hitPool.Init(512);
        damageNumberPool.Init(512);
        enemyPool.Init(512);

        Tileset::LoadTileset("assets/forest.til");
        Metasprite::LoadMetasprites("assets/meta.spr");
        Level::LoadLevels("assets/levels.lev");

        pNametables = Rendering::GetNametablePtr(pRenderContext, 0);

        // SETTINGS
        pRenderSettings = Rendering::GetSettingsPtr(pRenderContext);
	}

    void Free() {
        
    }

    r64 GetAverageFramerate(r64 dt) {
        r64 sum = 0.0;
        // Pop front
        for (u32 i = 1; i < SUCCESSIVE_FRAME_TIME_COUNT; i++) {
            sum += successiveFrameTimes[i];
            successiveFrameTimes[i - 1] = successiveFrameTimes[i];
        }
        // Push back
        sum += dt;
        successiveFrameTimes[SUCCESSIVE_FRAME_TIME_COUNT - 1] = dt;

        return (r64)SUCCESSIVE_FRAME_TIME_COUNT / sum;
    }

    void UpdateHUD(Rendering::RenderContext* pContext, r64 dt) {
        char hudText[128];
        const char* levelName = pCurrentLevel != nullptr ? pCurrentLevel->name : "NO LEVEL LOADED";
        snprintf(hudText, 64, " %4d FPS (%2d ms) (%04d, %04d) %s", (int)(averageFramerate), (int)(dt*1000), (int)viewport.x, (int)viewport.y, levelName);
        memcpy(&pNametables[0].tiles, hudText, 128);
    }

    void RenderHUD(Rendering::RenderContext* pContext) {
        Rendering::RenderState state = {
            0,
            0
        };
        Rendering::SetRenderState(pContext, 0, 16, state);
    }

    constexpr u8 bgChrSheetIndex = 0;
    constexpr u8 spriteChrSheetIndex = 1;

    constexpr u8 playerWingFrameBankOffsets[4] = { 0x00, 0x08, 0x10, 0x18 };
    constexpr u8 playerHeadFrameBankOffsets[9] = { 0x20, 0x24, 0x28, 0x30, 0x34, 0x38, 0x40, 0x44, 0x48 };
    constexpr u8 playerLegsFrameBankOffsets[4] = { 0x50, 0x54, 0x58, 0x5C };
    constexpr u8 playerHandFrameBankOffsets[3] = { 0x60, 0x62, 0x64 };
    constexpr u8 playerBowFrameBankOffsets[3] = { 0x68, 0x70, 0x78 };
    constexpr u8 playerLauncherFrameBankOffsets[3] = { 0x80, 0x88, 0x90 };

    constexpr u8 playerWingFrameChrOffset = 0x00;
    constexpr u8 playerHeadFrameChrOffset = 0x08;
    constexpr u8 playerLegsFrameChrOffset = 0x0C;
    constexpr u8 playerHandFrameChrOffset = 0x10;
    constexpr u8 playerWeaponFrameChrOffset = 0x12;

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

    IVec2 WorldPosToScreenPixels(Vec2 pos) {
        return IVec2{
            (s32)round((pos.x - viewport.x) * TILE_SIZE),
            (s32)round((pos.y - viewport.y) * TILE_SIZE)
        };
    }

    void DrawPlayer(Rendering::Sprite** ppNextSprite, r64 dt) {
        IVec2 drawPos = WorldPosToScreenPixels(Vec2{ playerState.x, playerState.y });
        drawPos.y += playerState.vOffset;

        // Draw weapon first
        IVec2 weaponOffset;
        u8 weaponFrameBankOffset;
        u32 weaponMetaspriteIndex;
        switch (playerState.weapon) {
        case WpnBow: {
            weaponOffset = playerBowOffsets[playerState.aMode];
            weaponFrameBankOffset = playerBowFrameBankOffsets[playerState.aMode];
            weaponMetaspriteIndex = playerState.aMode == AimFwd ? playerBowFwdMetaspriteIndex : playerBowDiagMetaspriteIndex;
            break;
        }
        case WpnLauncher: {
            weaponOffset = playerLauncherOffsets[playerState.aMode];
            weaponFrameBankOffset = playerLauncherFrameBankOffsets[playerState.aMode];
            weaponMetaspriteIndex = playerState.aMode == AimFwd ? playerLauncherFwdMetaspriteIndex : playerLauncherDiagMetaspriteIndex;
            break;
        }
        default:
            break;
        }
        weaponOffset.x *= playerState.direction;

        Rendering::Util::CopyChrTiles(playerBank.tiles + weaponFrameBankOffset, pChr[1].tiles + playerWeaponFrameChrOffset, playerWeaponFrameTileCount);

        const Metasprite::Metasprite& bowMetasprite = Metasprite::GetMetaspritesPtr()[weaponMetaspriteIndex];
        Rendering::Util::CopyMetasprite(bowMetasprite.spritesRelativePos, *ppNextSprite, bowMetasprite.spriteCount, drawPos + weaponOffset, playerState.direction == DirLeft, false);
        *ppNextSprite += bowMetasprite.spriteCount;

        // Animate chr sheet using player bank
        // TODO: Maybe do this on the GPU?
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerHandFrameBankOffsets[playerState.aMode],
            pChr[1].tiles + playerHandFrameChrOffset,
            playerHandFrameTileCount
        );
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerHeadFrameBankOffsets[playerState.aMode * 3 + playerState.hMode],
            pChr[1].tiles + playerHeadFrameChrOffset,
            playerHeadFrameTileCount
        );
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerLegsFrameBankOffsets[playerState.lMode],
            pChr[1].tiles + playerLegsFrameChrOffset,
            playerLegsFrameTileCount
        );
        Rendering::Util::CopyChrTiles(
            playerBank.tiles + playerWingFrameBankOffsets[playerState.wingFrame],
            pChr[1].tiles + playerWingFrameChrOffset,
            playerWingFrameTileCount
        );

        // Draw player
        Metasprite::Metasprite characterMetasprite = Metasprite::GetMetaspritesPtr()[playerState.aMode];
        Rendering::Util::CopyMetasprite(characterMetasprite.spritesRelativePos, *ppNextSprite, characterMetasprite.spriteCount, drawPos, playerState.direction == DirLeft, false);
        if (playerState.damageTimer > 0) {
            u8 damagePalette = (u8)(gameplaySecondsElapsed * 20) % 4;
            Rendering::Util::SetSpritesPalette(*ppNextSprite, characterMetasprite.spriteCount, damagePalette);
        }
        *ppNextSprite += characterMetasprite.spriteCount;
    }

    void DrawDamageNumbers(Rendering::Sprite** ppNextSprite) {
        char dmgStr[8]{};

        for (int i = 0; i < damageNumberPool.Count(); i++) {
            PoolHandle<DamageNumber> handle = damageNumberPool.GetHandle(i);
            DamageNumber* dmg = damageNumberPool[handle];

            _itoa_s(dmg->damage, dmgStr, 10);

            // Ascii character '0' = 0x30
            s32 chrOffset = 0x70 - 0x30;

            for (int i = 0; i < strlen(dmgStr); i++) {
                IVec2 pixelPos = WorldPosToScreenPixels(dmg->pos);
                const Rendering::Sprite sprite = {
                    pixelPos.y,
                    pixelPos.x + i * 5,
                    dmgStr[i] + chrOffset,
                    1
                };
                *((*ppNextSprite)++) = sprite;
            }
        }
    }

    void DrawArrows(Rendering::Sprite** ppNextSprite) {
        for (int i = 0; i < arrowPool.Count(); i++) {
            PoolHandle<Arrow> handle = arrowPool.GetHandle(i);
            Arrow* arrow = arrowPool[handle];
            if (arrow == nullptr) {
                continue;
            }

            IVec2 pixelPos = WorldPosToScreenPixels(arrow->pos);
            if (arrow->type == WpnBow) {
                bool hFlip = arrow->vel.x < 0.0f;
                bool vFlip = arrow->vel.y < -10.0f;
                u32 spriteIndex = abs(arrow->vel.y) < 10.0f ? playerBowArrowFwdMetaspriteIndex : playerBowArrowDiagMetaspriteIndex;
                Metasprite::Metasprite metasprite = Metasprite::GetMetaspritesPtr()[spriteIndex];
                Rendering::Util::CopyMetasprite(metasprite.spritesRelativePos, *ppNextSprite, metasprite.spriteCount, pixelPos, hFlip, vFlip);
                *ppNextSprite += metasprite.spriteCount;
            }
            else {
                Metasprite::Metasprite metasprite = Metasprite::GetMetaspritesPtr()[playerLauncherGrenadeMetaspriteIndex];
                const Vec2 dir = arrow->vel.Normalize();
                const r32 angle = atan2f(dir.y, dir.x);
                const s32 frameIndex = (s32)roundf(((angle + pi) / (pi*2)) * 7);
                Rendering::Util::CopyMetasprite(metasprite.spritesRelativePos + frameIndex, *ppNextSprite, 1, pixelPos, false, false);
                (*ppNextSprite)++;
            }
        }
    }

    void DrawHits(Rendering::Sprite** ppNextSprite) {
        for (int i = 0; i < hitPool.Count(); i++) {
            PoolHandle<Impact> handle = hitPool.GetHandle(i);
            Impact* impact = hitPool.Get(handle);

            u32 frame = (u32)((impact->accumulator / impactAnimLength) * impactFrameCount);
            IVec2 pixelPos = WorldPosToScreenPixels(impact->pos);

            Rendering::Sprite debugSprite = {
                pixelPos.y - (TILE_SIZE / 2),
                pixelPos.x - (TILE_SIZE / 2),
                0x20 + frame,
                1
            };
            *((*ppNextSprite)++) = debugSprite;
        }
    }

    void DrawShield(Rendering::Sprite** ppNextSprite, r64 dt) {
        // Draw shield around player
        static const r32 shieldRadius = 2.0f;
        static const u32 shieldCount = 4;
        static r32 shieldRot = 0.0f;

        const r32 tangentialVel = playerState.direction * 2.0f + playerState.hSpeed;
        shieldRot += tangentialVel / shieldRadius * dt;

        const Metasprite::Metasprite& shieldMetasprite = Metasprite::GetMetaspritesPtr()[7];
        const r32 angleOffset = pi * 2 / shieldCount;
        for (u32 i = 0; i < shieldCount; i++) {
            const r32 angle = shieldRot + angleOffset * i;
            Vec2 pos = { cos(angle) * shieldRadius + playerState.x, sin(angle) * shieldRadius + playerState.y };
            IVec2 drawPos = WorldPosToScreenPixels(pos);

            Rendering::Util::CopyMetasprite(shieldMetasprite.spritesRelativePos, *ppNextSprite, shieldMetasprite.spriteCount, drawPos, false, false);
            *ppNextSprite += shieldMetasprite.spriteCount;
        }
    }

    static void DrawEnemies(Rendering::Sprite** ppNextSprite, r64 dt) {
        Metasprite::Metasprite enemyMetasprite = Metasprite::GetMetaspritesPtr()[5];

        for (int i = 0; i < enemyPool.Count(); i++) {
            PoolHandle<Enemy> handle = enemyPool.GetHandle(i);
            Enemy* enemy = enemyPool.Get(handle);

            IVec2 enemyPixelPos = WorldPosToScreenPixels(enemy->pos);
            Rendering::Util::CopyMetasprite(enemyMetasprite.spritesRelativePos, *ppNextSprite, enemyMetasprite.spriteCount, enemyPixelPos, false, false);
            if (enemy->damageTimer > 0) {
                u8 damagePalette = (u8)(gameplaySecondsElapsed * 20) % 4;
                Rendering::Util::SetSpritesPalette(*ppNextSprite, enemyMetasprite.spriteCount, damagePalette);
            }
            *ppNextSprite += enemyMetasprite.spriteCount;
        }
    }

    // TODO: Sprite position update could be separate from all the other stuff like animation that is more like game logic
    // It would make it easier to pause gameplay logic and keep positions updating for the editor
    void Render(Rendering::RenderContext* pRenderContext, r64 dt) {
        Rendering::Util::ClearSprites(pSprites, 256);
        Rendering::Sprite* pNextSprite = pSprites;
        
        DrawDamageNumbers(&pNextSprite);
        //DrawShield(&pNextSprite, dt);
        DrawPlayer(&pNextSprite, dt);
        DrawArrows(&pNextSprite);
        DrawHits(&pNextSprite);
        DrawEnemies(&pNextSprite, dt);

        //UpdateHUD(pRenderContext, dt);
        //RenderHUD(pRenderContext);

        /*for (int i = 0; i < 272; i++) {
            float sine = sin(gameplaySecondsElapsed + (i / 8.0f));
            Rendering::RenderState state = {
                (s32)((viewport.x + sine) * TILE_SIZE),
                (s32)(viewport.y * TILE_SIZE)
            };
            Rendering::SetRenderState(pRenderContext, 16 + i, 1, state);
        }*/

        Rendering::RenderState state = {
            (s32)(viewport.x * TILE_SIZE),
            (s32)(viewport.y* TILE_SIZE)
        };
        // TODO: Refactor this so I can get rid of render context as a parameter
        Rendering::SetRenderState(pRenderContext, 0, 288, state);
    }

    constexpr Vec2 viewportScrollThreshold = { 8.0f, 6.0f };

    static void UpdateViewport(bool loadTiles = true) {
        Vec2 viewportCenter = Vec2{ viewport.x + VIEWPORT_WIDTH_TILES / 2.0f, viewport.y + VIEWPORT_HEIGHT_TILES / 2.0f };
        Vec2 playerOffset = Vec2{ playerState.x - viewportCenter.x, playerState.y - viewportCenter.y };

        Vec2 delta = { 0.0f, 0.0f };
        if (playerOffset.x > viewportScrollThreshold.x) {
            delta.x = playerOffset.x - viewportScrollThreshold.x;
        }
        else if (playerOffset.x < -viewportScrollThreshold.x) {
            delta.x = playerOffset.x + viewportScrollThreshold.x;
        }

        if (playerOffset.y > viewportScrollThreshold.y) {
            delta.y = playerOffset.y - viewportScrollThreshold.y;
        }
        else if (playerOffset.y < -viewportScrollThreshold.y) {
            delta.y = playerOffset.y + viewportScrollThreshold.y;
        }

        MoveViewport(&viewport, pNametables, pCurrentLevel, delta.x, delta.y, loadTiles);
    }

    static void TriggerLevelTransition(bool direction = true) {
        state = StateLevelTransition;

        static constexpr u32 bufferZoneWidth = 1;

        const IVec2 viewportPosInMetatiles = Level::WorldToTilemap({ viewport.x, viewport.y });
        // Window a bit larger than viewport to ensure full coverage
        const IVec2 transitionWindowPos = viewportPosInMetatiles - IVec2{ bufferZoneWidth, bufferZoneWidth };
        const IVec2 playerPosInMetatiles = Level::WorldToTilemap({ playerState.x, playerState.y });
        const IVec2 center = playerPosInMetatiles - transitionWindowPos;

        levelTransitionState.origin = center;
        levelTransitionState.windowWorldPos = transitionWindowPos;
        const IVec2 transitionWindowSize = IVec2{ viewportWidthInMetatiles + bufferZoneWidth * 2, viewportHeightInMetatiles + bufferZoneWidth * 2 };
        levelTransitionState.windowSize = transitionWindowSize;

        const u32 stepsToTopLeft = center.x + center.y;
        const u32 stepsToTopRight = (transitionWindowSize.x - center.x) + center.y;
        const u32 stepsToBtmLeft = center.x + (transitionWindowSize.y - center.y);
        const u32 stepsToBtmRight = (transitionWindowSize.x - center.x) + (transitionWindowSize.y - center.y);
        const u32 maxSteps = Max(Max(Max(stepsToTopLeft, stepsToTopRight), stepsToBtmLeft), stepsToBtmRight);

        levelTransitionState.steps = maxSteps;
        levelTransitionState.currentStep = 0;

        static constexpr r32 duration = 0.8f;

        levelTransitionState.stepDuration = duration / maxSteps;
        levelTransitionState.accumulator = 0.0f;

        levelTransitionState.direction = direction;
    }

    static void UpdateLevelTransition(Rendering::RenderContext* pRenderContext, r64 dt) {

        if (levelTransitionState.currentStep >= levelTransitionState.steps) {

            if (levelTransitionState.direction) {
                // This could be optimized because it's really stupid but all nametables need to be filled with this tile for the transition to work
                Tileset::FillAllNametablesWithMetatile(pNametables, 16);

                // Move viewport and sprites before next transition
                LoadLevel(nextLevel, nextLevelScreenIndex, false);
                ReloadLevel(false);
                UpdateViewport(false);
                Render(pRenderContext, dt);

                TriggerLevelTransition(false);
            }
            else {
                state = StatePlaying;
                RefreshViewport(&viewport, pNametables, pCurrentLevel);
            }

            return;
        }

        levelTransitionState.accumulator += dt;

        while (levelTransitionState.accumulator >= levelTransitionState.stepDuration) {
            levelTransitionState.accumulator -= levelTransitionState.stepDuration;

            for (s32 row = 0; row < levelTransitionState.windowSize.y; row++) {

                const s32 metatileY = row + levelTransitionState.windowWorldPos.y;

                for (s32 col = 0; col < levelTransitionState.windowSize.x; col++) {

                    const s32 metatileX = col + levelTransitionState.windowWorldPos.x;

                    if (!Level::TileInLevelBounds(pCurrentLevel, { metatileX, metatileY })) {
                        continue;
                    }

                    // Calculate the distance from the center
                    int distance = abs(row - levelTransitionState.origin.y) + abs(col - levelTransitionState.origin.x);

                    // Check if this tile should transition on this step
                    bool shouldTransition = levelTransitionState.direction ? (levelTransitionState.currentStep == levelTransitionState.steps - distance) : (levelTransitionState.currentStep == distance);

                    if (shouldTransition) {
                        const u32 screenIndex = Level::TilemapToScreenIndex(pCurrentLevel, { metatileX, metatileY });
                        if (screenIndex >= pCurrentLevel->screenCount) {
                            continue;
                        }
                        const u32 nametableInd = screenIndex % NAMETABLE_COUNT;

                        const u32 screenTileIndex = Level::TilemapToMetatileIndex({ metatileX, metatileY });
                        const u8 metatileIndex = pCurrentLevel->screens[screenIndex].tiles[screenTileIndex].metatile;
                        const Vec2 screenTilePos = Level::TileIndexToScreenOffset(screenTileIndex);

                        if (levelTransitionState.direction) {
                            Tileset::CopyMetatileToNametable(&pNametables[nametableInd], (u16)screenTilePos.x, (u16)screenTilePos.y, 16);
                        }
                        else {
                            Tileset::CopyMetatileToNametable(&pNametables[nametableInd], (u16)screenTilePos.x, (u16)screenTilePos.y, metatileIndex);
                        }
                    }
                }
            }

            levelTransitionState.currentStep++;
        }
    }

    void PlayerInput(r32 dt) {
        if (Input::Down(Input::DPadLeft)) {
            playerState.direction = DirLeft;
            playerState.hSpeed = -12.5f;
        }
        else if (Input::Down(Input::DPadRight)) {
            playerState.direction = DirRight;
            playerState.hSpeed = 12.5f;
        }
        else {
            playerState.hSpeed = 0;
        }

        // Aim mode
        if (Input::Down(Input::DPadUp)) {
            playerState.aMode = AimUp;
        }
        else if (Input::Down(Input::DPadDown)) {
            playerState.aMode = AimDown;
        }
        else playerState.aMode = AimFwd;

        if (Input::Pressed(Input::Start)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if (Input::Pressed(Input::A) && (!playerState.inAir || !playerState.doubleJumped)) {
            playerState.vSpeed = -31.25f;
            if (playerState.inAir) {
                playerState.doubleJumped = true;
            }

            // Trigger new flap
            playerState.wingFrame++;
        }

        if (playerState.vSpeed < 0 && Input::Released(Input::A)) {
            playerState.vSpeed /= 2;
        }

        playerState.slowFall = true;
        if (Input::Up(Input::A) || playerState.vSpeed < 0) {
            playerState.slowFall = false;
        }

        if (Input::Released(Input::B)) {
            shootTimer = 0.0f;
        }

        if (Input::Pressed(Input::Select)) {
            if (playerState.weapon == WpnLauncher) {
                playerState.weapon = WpnBow;
            }
            else playerState.weapon = WpnLauncher;
        }

        // Enter door
        if (Input::Pressed(Input::DPadUp) && !playerState.inAir) {
            const Vec2 checkPos = { playerState.x, playerState.y + 1.0f };
            const u32 screenInd = Level::WorldToScreenIndex(pCurrentLevel, checkPos);
            const u32 tileInd = Level::WorldToMetatileIndex(checkPos);
            const Level::Screen& screen = pCurrentLevel->screens[screenInd];
            if (screen.tiles[tileInd].actorType == Level::ACTOR_DOOR) {
                nextLevel = screen.exitTargetLevel;
                nextLevelScreenIndex = screen.exitTargetScreen;
                TriggerLevelTransition();
            }
        }
    }

    void PlayerBgCollision(r64 dt, Rendering::RenderContext* pRenderContext) {
        if (playerState.slowFall) {
            playerState.vSpeed += (gravity / 4) * dt;
        }
        else {
            playerState.vSpeed += gravity * dt;
        }

        Metasprite::Metasprite characterMetasprite = Metasprite::GetMetaspritesPtr()[0];
        Collision::Collider hitbox = characterMetasprite.colliders[0];
        Vec2 hitboxPos = Vec2{ playerState.x + hitbox.xOffset, playerState.y + hitbox.yOffset };
        Vec2 hitboxDimensions = Vec2{ hitbox.width, hitbox.height };
        r32 dx = playerState.hSpeed * dt;

        Collision::HitResult hit{};
        Collision::SweepBoxHorizontal(pCurrentLevel, hitboxPos, hitboxDimensions, dx, hit);
        playerState.x = hit.location.x;
        if (hit.blockingHit) {
            playerState.hSpeed = 0;
        }

        r32 dy = playerState.vSpeed * dt;
        hitboxPos = Vec2{ playerState.x + hitbox.xOffset, playerState.y + hitbox.yOffset };
        Collision::SweepBoxVertical(pCurrentLevel, hitboxPos, hitboxDimensions, dy, hit);
        playerState.y = hit.location.y;
        if (hit.blockingHit) {
            // If ground
            if (playerState.vSpeed > 0) {
                playerState.inAir = false;
                playerState.doubleJumped = false;
            }

            playerState.vSpeed = 0;
        }
        else {
            // TODO: Add coyote time
            playerState.inAir = true;
        }
    }

    void PlayerShoot(r64 dt) {
        if (shootTimer > dt) {
            shootTimer -= dt;
        }
        else shootTimer = 0.0f;

        if (Input::Down(Input::B) && shootTimer <= 0.0f) {
            shootTimer += shootDelay;

            PoolHandle<Arrow> handle = arrowPool.Add();
            Arrow* arrow = arrowPool[handle];
            if (arrow != nullptr) {
                const Vec2 fwdOffset = Vec2{ 0.75f * playerState.direction, -0.5f };
                const Vec2 upOffset = Vec2{ 0.375f * playerState.direction, -1.0f };
                const Vec2 downOffset = Vec2{ 0.5f * playerState.direction, -0.25f };

                arrow->pos = Vec2{ playerState.x, playerState.y };
                arrow->vel = Vec2{};
                arrow->type = playerState.weapon;
                arrow->bounces = 10;

                if (playerState.aMode == AimFwd) {
                    arrow->pos = arrow->pos + fwdOffset;
                    arrow->vel.x = 80.0f * playerState.direction;
                }
                else {
                    arrow->vel.x = 56.56f * playerState.direction;
                    arrow->vel.y = (playerState.aMode == AimUp) ? -56.56f : 56.56f;
                    arrow->pos = arrow->pos + ((playerState.aMode == AimUp) ? upOffset : downOffset);
                }

                if (playerState.weapon == WpnLauncher) {
                    arrow->vel = arrow->vel * 0.75f;
                }
                arrow->vel = arrow->vel + Vec2{ playerState.hSpeed, playerState.vSpeed } *dt;
            }
        }
    }

    void PlayerAnimate(r64 dt) {
        // Legs mode
        if (playerState.vSpeed < 0) {
            playerState.lMode = LegsJump;
        }
        else if (playerState.vSpeed > 0) {
            playerState.lMode = LegsFall;
        }
        else if (abs(playerState.hSpeed) > 0) {
            playerState.lMode = LegsFwd;
        }
        else {
            playerState.lMode = LegsIdle;
        }

        // Head mode
        if (playerState.vSpeed > 0 && !playerState.slowFall) {
            playerState.hMode = HeadFall;
        }
        else if (abs(playerState.hSpeed) > 0) {
            playerState.hMode = HeadFwd;
        }
        else {
            playerState.hMode = HeadIdle;
        }

        // Wing mode
        if (playerState.vSpeed < 0) {
            playerState.wMode = WingJump;
        }
        else if (playerState.vSpeed > 0 && !playerState.slowFall) {
            playerState.wMode = WingFall;
        }
        else {
            playerState.wMode = WingFlap;
        }

        // Wing flapping
        const r32 wingAnimFrameLength = playerState.wMode == WingFlap ? 0.18f : 0.09f;
        playerState.wingCounter += dt / wingAnimFrameLength;
        while (playerState.wingCounter > 1.0f) {
            if (!(playerState.wMode == WingJump && playerState.wingFrame == 2) && !((playerState.wMode == WingFall && playerState.wingFrame == 0))) {
                playerState.wingFrame++;
            }
            playerState.wingCounter -= 1.0f;
        }
        playerState.wingFrame %= 4;

        if (playerState.vSpeed == 0) {
            playerState.vOffset = playerState.wingFrame > 1 ? -1 : 0;
        }
        else {
            playerState.vOffset = 0;
        }
    }

    void Step(r64 dt, Rendering::RenderContext* pRenderContext) {
        secondsElapsed += dt;
        averageFramerate = GetAverageFramerate(dt);

        // TODO: Move out of game logic
        Input::Poll();

        if (state == StateLevelTransition) {
            UpdateLevelTransition(pRenderContext, dt);
        }
        else {
            if (!paused) {
                gameplaySecondsElapsed += dt;

                PlayerInput(dt);
                PlayerBgCollision(dt, pRenderContext);
                PlayerShoot(dt);
                PlayerAnimate(dt);

                if (playerState.damageTimer > dt) {
                    playerState.damageTimer -= dt;
                }
                else playerState.damageTimer = 0.0f;

                // Update arrows
                for (int i = 0; i < arrowPool.Count(); i++) {
                    PoolHandle<Arrow> handle = arrowPool.GetHandle(i);
                    Arrow* arrow = arrowPool[handle];
                    if (arrow == nullptr) {
                        continue;
                    }

                    // TODO: Object struct and updater that does this for everything?
                    bool hFlip = arrow->vel.x < 0.0f;
                    bool vFlip = arrow->vel.y < -10.0f;

                    u32 spriteIndex = abs(arrow->vel.y) < 10.0f ? 3 : 4;
                    Metasprite::Metasprite metasprite = Metasprite::GetMetaspritesPtr()[spriteIndex];

                    Collision::Collider hitbox = metasprite.colliders[0];
                    Vec2 hitboxPos = Vec2{ arrow->pos.x + hitbox.xOffset * (hFlip ? -1.0f : 1.0f), arrow->pos.y + hitbox.yOffset * (vFlip ? -1.0f : 1.0f) };
                    Vec2 hitboxDimensions = Vec2{ hitbox.width, hitbox.height };

                    r32 dx = arrow->vel.x * dt;

                    Collision::HitResult hit{};
                    Collision::SweepBoxHorizontal(pCurrentLevel, hitboxPos, hitboxDimensions, dx, hit);
                    if (hit.blockingHit) {
                        if (arrow->type == WpnBow || arrow->bounces == 0) {
                            arrowPool.Remove(handle);
                            PoolHandle<Impact> hitHandle = hitPool.Add();
                            Impact* impact = hitPool[hitHandle];
                            impact->pos = hit.impactPoint;
                            impact->accumulator = 0.0f;
                            continue;
                        }
                        arrow->pos = hit.location;
                        arrow->vel.x *= -1.0f;
                        arrow->bounces--;
                    } else arrow->pos.x += dx;

                    if (arrow->type == WpnLauncher) {
                        arrow->vel.y += dt * gravity * 4.0f;
                    }
                    r32 dy = arrow->vel.y * dt;
                    hitboxPos = Vec2{ arrow->pos.x + hitbox.xOffset * (hFlip ? -1.0f : 1.0f), arrow->pos.y + hitbox.yOffset * (vFlip ? -1.0f : 1.0f) };
                    Collision::SweepBoxVertical(pCurrentLevel, hitboxPos, hitboxDimensions, dy, hit);
                    if (hit.blockingHit) {
                        if (arrow->type == WpnBow || arrow->bounces == 0) {
                            arrowPool.Remove(handle);
                            PoolHandle<Impact> hitHandle = hitPool.Add();
                            Impact* impact = hitPool[hitHandle];
                            impact->pos = hit.impactPoint;
                            impact->accumulator = 0.0f;
                            continue;
                        }
                        arrow->pos = hit.location;
                        arrow->vel.y *= -1.0f;
                        arrow->bounces--;
                    } else arrow->pos.y += dy;

                    hitboxPos = Vec2{ arrow->pos.x + hitbox.xOffset * (hFlip ? -1.0f : 1.0f), arrow->pos.y + hitbox.yOffset * (vFlip ? -1.0f : 1.0f) };

                    // Collision with enemy
                    // TODO: Need a smarter collision system soon...
                    for (int e = 0; e < enemyPool.Count(); e++) {
                        static const Metasprite::Metasprite enemyMetasprite = Metasprite::GetMetaspritesPtr()[5];
                        static const Collision::Collider enemyHitbox = enemyMetasprite.colliders[0];

                        PoolHandle<Enemy> enemyHandle = enemyPool.GetHandle(e);
                        Enemy* enemy = enemyPool.Get(enemyHandle);

                        const Vec2 enemyHitboxPos = Vec2{ enemy->pos.x + enemyHitbox.xOffset, enemy->pos.y + enemyHitbox.yOffset };
                        const Vec2 enemyHitboxDimensions = Vec2{ enemyHitbox.width, enemyHitbox.height };

                        if (hitboxPos.x - hitboxDimensions.x / 2.0f < enemyHitboxPos.x + enemyHitboxDimensions.x / 2.0f &&
                            hitboxPos.x + hitboxDimensions.x / 2.0f > enemyHitboxPos.x - enemyHitboxDimensions.x / 2.0f &&
                            hitboxPos.y - hitboxDimensions.y / 2.0f < enemyHitboxPos.y + enemyHitboxDimensions.y / 2.0f &&
                            hitboxPos.y + hitboxDimensions.y / 2.0f > enemyHitboxPos.y - enemyHitboxDimensions.y / 2.0f) {

                            arrowPool.Remove(handle);
                            PoolHandle<Impact> hitHandle = hitPool.Add();
                            Impact* impact = hitPool[hitHandle];
                            impact->pos = arrow->pos;
                            impact->accumulator = 0.0f;

                            s32 damage = (rand() % 2) + 1;
                            enemy->health -= damage;
                            enemy->damageTimer = damageDelay;

                            // Add damage numbers
                            // Random point inside enemy hitbox
                            Vec2 dmgPos = Vec2{ (r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.x) + enemyHitboxPos.x - enemyHitboxDimensions.x / 2.0f, (r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.y) + enemyHitboxPos.y - enemyHitboxDimensions.y / 2.0f };

                            PoolHandle<DamageNumber> dmgHandle = damageNumberPool.Add();
                            DamageNumber* dmgNumber = damageNumberPool[dmgHandle];
                            dmgNumber->damage = damage;
                            dmgNumber->pos = dmgPos;
                            dmgNumber->accumulator = 0.0f;

                            continue;
                        }
                    }

                    if (arrow->pos.x < viewport.x || arrow->pos.x > viewport.x + VIEWPORT_WIDTH_TILES || arrow->pos.y < viewport.y || arrow->pos.y > viewport.y + VIEWPORT_HEIGHT_TILES) {
                        arrowPool.Remove(handle);
                    }
                }

                // Update explosions
                for (int i = 0; i < hitPool.Count(); i++) {
                    PoolHandle<Impact> handle = hitPool.GetHandle(i);
                    Impact* impact = hitPool[handle];
                    impact->accumulator += dt;

                    if (impact->accumulator >= impactAnimLength) {
                        hitPool.Remove(handle);
                        continue;
                    }
                }

                // Update damage numbers
                r32 dmgNumberVel = -3.0f;
                for (int i = 0; i < damageNumberPool.Count(); i++) {
                    PoolHandle<DamageNumber> handle = damageNumberPool.GetHandle(i);
                    DamageNumber* dmgNumber = damageNumberPool[handle];
                    dmgNumber->accumulator += dt;

                    if (dmgNumber->accumulator >= damageNumberLifetime) {
                        damageNumberPool.Remove(handle);
                        continue;
                    }

                    dmgNumber->pos.y += dmgNumberVel * dt;
                }

                // Update enemies
                for (int i = 0; i < enemyPool.Count(); i++) {
                    PoolHandle<Enemy> handle = enemyPool.GetHandle(i);
                    Enemy* enemy = enemyPool.Get(handle);

                    if (enemy->health <= 0) {
                        enemyPool.Remove(handle);
                        continue;
                    }

                    static const r32 amplitude = 7.5f;
                    r32 sineTime = sin(gameplaySecondsElapsed);
                    enemy->pos.y = enemy->spawnHeight + sineTime * amplitude;

                    // Player take damage
                    if (playerState.damageTimer == 0) {
                        static const Metasprite::Metasprite enemyMetasprite = Metasprite::GetMetaspritesPtr()[5];
                        static const Collision::Collider enemyHitbox = enemyMetasprite.colliders[0];

                        const Vec2 hitboxPos = Vec2{ enemy->pos.x + enemyHitbox.xOffset, enemy->pos.y + enemyHitbox.yOffset };
                        const Vec2 hitboxDimensions = Vec2{ enemyHitbox.width, enemyHitbox.height };

                        const Metasprite::Metasprite characterMetasprite = Metasprite::GetMetaspritesPtr()[0];
                        const Collision::Collider playerHitbox = characterMetasprite.colliders[0];

                        const Vec2 playerHitboxPos = Vec2{ playerState.x + playerHitbox.xOffset, playerState.y + playerHitbox.yOffset };
                        const Vec2 playerHitboxDimensions = Vec2{ playerHitbox.width, playerHitbox.height };

                        if (hitboxPos.x - hitboxDimensions.x / 2.0f < playerHitboxPos.x + playerHitboxDimensions.x / 2.0f &&
                            hitboxPos.x + hitboxDimensions.x / 2.0f > playerHitboxPos.x - playerHitboxDimensions.x / 2.0f &&
                            hitboxPos.y - hitboxDimensions.y / 2.0f < playerHitboxPos.y + playerHitboxDimensions.y / 2.0f &&
                            hitboxPos.y + hitboxDimensions.y / 2.0f > playerHitboxPos.y - playerHitboxDimensions.y / 2.0f) {

                            playerState.damageTimer = damageDelay;
                            // playerState.hSpeed = -8.0f * playerState.direction;
                            // playerState.vSpeed = -16.0f;
                        }
                    }

                    if (enemy->damageTimer > dt) {
                        enemy->damageTimer -= dt;
                    }
                    else enemy->damageTimer = 0.0f;
                }

                UpdateViewport();
            }

            // TODO: Maybe change the name of this? (It's not actually rendering, just setting up render state)
            Render(pRenderContext, dt);
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
    Level::Level* GetLevel() {
        return pCurrentLevel;
    }
}