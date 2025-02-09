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
    s32 enterScreenIndex = -1;

    // This is pretty ugly...
    u32 nextLevel = 0;
    s32 nextLevelScreenIndex = -1;

    struct LevelTransitionState {
        Vec2 origin;
        Vec2 windowWorldPos;
        Vec2 windowSize;

        u32 steps;
        u32 currentStep;
        r32 stepDuration;

        r32 accumulator;
        bool direction;
    };

    LevelTransitionState levelTransitionState;

    Pool<Actor> actors;
    Actor* pViewportTarget = nullptr;

    r32 gravity = 35;

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

    ChrSheet playerBank;

    bool paused = false;

    constexpr r32 damageDelay = 0.5f;

#pragma region Actors
    static void InitializeActor(Actor* pActor) {
        switch (pActor->pPrototype->behaviour) {
        case ACTOR_BEHAVIOUR_PLAYER: {
            pActor->playerState.direction = DirRight;
            pActor->playerState.weapon = WpnLauncher;
            break;
        }
        case ACTOR_BEHAVIOUR_ENEMY_SKULL: {
            pActor->enemyState.baseHeight = pActor->position.y;
            pActor->enemyState.health = 10;
            break;
        }
        default:
            break;
        }

        pActor->drawData = ActorDrawData{};
    }

    static Actor* SpawnActor(const Actor* pTemplate) {
        PoolHandle<Actor> handle = actors.Add();
        Actor* pActor = actors.Get(handle);

        if (pActor == nullptr) {
            return nullptr;
        }

        *pActor = *pTemplate;
        InitializeActor(pActor);
        return pActor;
    }

    static Actor* SpawnActor(u32 presetIndex) {
        PoolHandle<Actor> handle = actors.Add();
        Actor* pActor = actors.Get(handle);

        if (pActor == nullptr) {
            return nullptr;
        }

        pActor->pPrototype = Actors::GetPrototype(presetIndex);
        InitializeActor(pActor);
        return pActor;
    }
#pragma endregion

    static void UpdateScreenScroll() {
        const Scanline state = {
            (s32)(viewport.x * METATILE_DIM_PIXELS),
            (s32)(viewport.y * METATILE_DIM_PIXELS)
        };
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = state;
        }
    }

    void LoadLevel(u32 index, s32 screenIndex, bool refresh) {
        if (index >= MAX_LEVEL_COUNT) {
            DEBUG_ERROR("Level count exceeded!");
        }

        pCurrentLevel = Levels::GetLevelsPtr() + index;
        enterScreenIndex = screenIndex;

        viewport.x = 0;
        viewport.y = 0;
        UpdateScreenScroll();

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
    }

    static bool SpawnAtFirstDoor(u32 screenIndex) {
        /*if (screenIndex >= pCurrentLevel->width * pCurrentLevel->height) {
            return false;
        }

        const Screen& screen = pCurrentLevel->screens[screenIndex];
        for (u32 i = 0; i < VIEWPORT_WIDTH_METATILES * VIEWPORT_HEIGHT_METATILES; i++) {
            const LevelTile& tile = screen.tiles[i];

            if (tile.actorType == ACTOR_DOOR) {
                const Vec2 screenRelativePos = TileIndexToScreenOffset(i);
                const Vec2 worldPos = ScreenOffsetToWorld(pCurrentLevel, screenRelativePos, screenIndex);

                playerState.x = worldPos.x + 1.0f;
                playerState.y = worldPos.y;

                return true;
            }
        }*/

        return false;
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
        pViewportTarget = nullptr;

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
            PoolHandle<Actor> handle = pCurrentLevel->actors.GetHandle(i);
            const Actor* pActor = pCurrentLevel->actors.Get(handle);

            Actor* pSpawned = SpawnActor(pActor);

            if (pSpawned->pPrototype->behaviour == ACTOR_BEHAVIOUR_PLAYER) {
                pViewportTarget = pSpawned;
            }
        }

        gameplaySecondsElapsed = 0.0f;

        if (refresh) {
            RefreshViewport(&viewport, pNametables, &pCurrentLevel->tilemap);
        }
    }

    u8 basePaletteColors[PALETTE_MEMORY_SIZE];

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

        hitPool.Init(512);
        damageNumberPool.Init(512);

        Tiles::LoadTileset("assets/forest.til");
        Metasprites::Load("assets/meta.spr");
        Levels::LoadLevels("assets/levels.lev");
        Actors::LoadPrototypes("assets/actors.pfb");

        // Initialize scanline state
        for (int i = 0; i < SCANLINE_COUNT; i++) {
            pScanlines[i] = { 0, 0 };
        }

        viewport.x = 0.0f;
        viewport.y = 0.0f;

        actors.Init(512);

        // TODO: Level should load palettes and tileset?
        LoadLevel(0);
	}

    void Free() {
        
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

    constexpr Vec2 viewportScrollThreshold = { 4.0f, 3.0f };

    static void UpdateViewport(bool loadTiles = true) {
        if (pViewportTarget == nullptr) {
            return;
        }

        Vec2 viewportCenter = Vec2{ viewport.x + VIEWPORT_WIDTH_METATILES / 2.0f, viewport.y + VIEWPORT_HEIGHT_METATILES / 2.0f };
        Vec2 targetOffset = pViewportTarget->position - viewportCenter;

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

        MoveViewport(&viewport, pNametables, &pCurrentLevel->tilemap, delta.x, delta.y, loadTiles);
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

    static void DrawActorFrame(const Actor* pActor, const s32 frameIndex, Sprite** ppNextSprite) {
        // Culling
        if (!PositionInViewportBounds(pActor->position)) {
            return;
        }

        const ActorDrawData& drawData = pActor->drawData;
        IVec2 drawPos = WorldPosToScreenPixels(pActor->position) + drawData.pixelOffset;
        const ActorAnimFrame& frame = pActor->pPrototype->pFrames[frameIndex];

        switch (pActor->pPrototype->animMode) {
        case ACTOR_ANIM_MODE_SPRITES: {
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(frame.metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos + frame.spriteIndex, *ppNextSprite, 1, drawPos, drawData.hFlip, drawData.vFlip, drawData.paletteOverride);
            (*ppNextSprite)++;
            break;
        }
        case ACTOR_ANIM_MODE_METASPRITES: {
            const Metasprite* pMetasprite = Metasprites::GetMetasprite(frame.metaspriteIndex);
            Rendering::Util::CopyMetasprite(pMetasprite->spritesRelativePos, *ppNextSprite, pMetasprite->spriteCount, drawPos, drawData.hFlip, drawData.vFlip, drawData.paletteOverride);
            *ppNextSprite += pMetasprite->spriteCount;
            break;
        }
        default:
            break;
        }
    }

    static void DrawPlayer(Actor* pPlayer, Sprite** ppNextSprite, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;

        ActorDrawData& drawData = pPlayer->drawData;
        drawData.hFlip = playerState.direction == DirLeft;
        drawData.paletteOverride = (playerState.damageTimer > 0) ? (s32)(gameplaySecondsElapsed * 20) % 4 : -1;
        if (playerState.velocity.y == 0) {
            drawData.pixelOffset.y = playerState.wingFrame > 1 ? -1 : 0;
        }
        else {
            drawData.pixelOffset.y = 0;
        }

        IVec2 drawPos = WorldPosToScreenPixels(pPlayer->position);
        drawPos = drawPos + drawData.pixelOffset;

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

        const Metasprite* bowMetasprite = Metasprites::GetMetasprite(weaponMetaspriteIndex);
        Rendering::Util::CopyMetasprite(bowMetasprite->spritesRelativePos, *ppNextSprite, bowMetasprite->spriteCount, drawPos + weaponOffset, drawData.hFlip, drawData.vFlip);
        *ppNextSprite += bowMetasprite->spriteCount;

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

        DrawActorFrame(pPlayer, playerState.aMode, ppNextSprite);
    }

    void DrawDamageNumbers(Sprite** ppNextSprite) {
        char dmgStr[8]{};

        for (int i = 0; i < damageNumberPool.Count(); i++) {
            PoolHandle<DamageNumber> handle = damageNumberPool.GetHandle(i);
            DamageNumber* dmg = damageNumberPool[handle];

            _itoa_s(dmg->damage, dmgStr, 10);

            // Ascii character '0' = 0x30
            s32 chrOffset = 0x70 - 0x30;

            for (int i = 0; i < strlen(dmgStr); i++) {
                IVec2 pixelPos = WorldPosToScreenPixels(dmg->pos);
                const Sprite sprite = {
                    pixelPos.y,
                    pixelPos.x + i * 5,
                    dmgStr[i] + chrOffset,
                    1
                };
                *((*ppNextSprite)++) = sprite;
            }
        }
    }

    void DrawHits(Sprite** ppNextSprite) {
        for (int i = 0; i < hitPool.Count(); i++) {
            PoolHandle<Impact> handle = hitPool.GetHandle(i);
            Impact* impact = hitPool.Get(handle);

            u32 frame = (u32)((impact->accumulator / impactAnimLength) * impactFrameCount);
            IVec2 pixelPos = WorldPosToScreenPixels(impact->pos);

            Sprite debugSprite = {
                pixelPos.y - (TILE_DIM_PIXELS / 2),
                pixelPos.x - (TILE_DIM_PIXELS / 2),
                0x20 + frame,
                1
            };
            *((*ppNextSprite)++) = debugSprite;
        }
    }

    /*void DrawShield(Sprite** ppNextSprite, r64 dt) {
        // Draw shield around player
        static const r32 shieldRadius = 2.0f;
        static const u32 shieldCount = 4;
        static r32 shieldRot = 0.0f;

        const r32 tangentialVel = playerState.direction * 2.0f + playerState.hSpeed;
        shieldRot += tangentialVel / shieldRadius * dt;

        const Metasprite* shieldMetasprite = Metasprites::GetMetasprite(7);
        const r32 angleOffset = pi * 2 / shieldCount;
        for (u32 i = 0; i < shieldCount; i++) {
            const r32 angle = shieldRot + angleOffset * i;
            Vec2 pos = { cos(angle) * shieldRadius + playerState.x, sin(angle) * shieldRadius + playerState.y };
            IVec2 drawPos = WorldPosToScreenPixels(pos);

            Rendering::Util::CopyMetasprite(shieldMetasprite->spritesRelativePos, *ppNextSprite, shieldMetasprite->spriteCount, drawPos, false, false);
            *ppNextSprite += shieldMetasprite->spriteCount;
        }
    }*/

    static void PlayerInput(Actor* pPlayer, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;
        if (Input::ButtonDown(BUTTON_DPAD_LEFT)) {
            playerState.direction = DirLeft;
            playerState.velocity.x = -6.25f;
        }
        else if (Input::ButtonDown(BUTTON_DPAD_RIGHT)) {
            playerState.direction = DirRight;
            playerState.velocity.x = 6.25f;
        }
        else {
            playerState.velocity.x = 0;
        }

        // Aim mode
        if (Input::ButtonDown(BUTTON_DPAD_UP)) {
            playerState.aMode = AimUp;
        }
        else if (Input::ButtonDown(BUTTON_DPAD_DOWN)) {
            playerState.aMode = AimDown;
        }
        else playerState.aMode = AimFwd;

        if (Input::ButtonPressed(BUTTON_START)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if (Input::ButtonPressed(BUTTON_A) && (!playerState.inAir || !playerState.doubleJumped)) {
            playerState.velocity.y = -15.625f;
            if (playerState.inAir) {
                playerState.doubleJumped = true;
            }

            // Trigger new flap
            playerState.wingFrame++;
        }

        if (playerState.velocity.y < 0 && Input::ButtonReleased(BUTTON_A)) {
            playerState.velocity.y /= 2;
        }

        playerState.slowFall = true;
        if (Input::ButtonUp(BUTTON_A) || playerState.velocity.y < 0) {
            playerState.slowFall = false;
        }

        if (Input::ButtonReleased(BUTTON_B)) {
            playerState.shootTimer = 0.0f;
        }

        if (Input::ButtonPressed(BUTTON_SELECT)) {
            if (playerState.weapon == WpnLauncher) {
                playerState.weapon = WpnBow;
            }
            else playerState.weapon = WpnLauncher;
        }

        // Enter door
        /*if (Input::ButtonPressed(BUTTON_DPAD_UP) && !playerState.inAir) {
            const Vec2 checkPos = { playerState.x, playerState.y + 1.0f };
            const u32 screenInd = Tiles::WorldToScreenIndex(&pCurrentLevel->tilemap, checkPos);
            const u32 tileInd = Tiles::WorldToMetatileIndex(checkPos);
            const Screen& screen = pCurrentLevel->tilemap.pScreens[screenInd];
            if (screen.tiles[tileInd].actorType == ACTOR_DOOR) {
                nextLevel = screen.exitTargetLevel;
                nextLevelScreenIndex = screen.exitTargetScreen;
                TriggerLevelTransition();
            }
        }*/
    }

    static void PlayerBgCollision(Actor* pPlayer, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;
        const AABB& hitbox = pPlayer->pPrototype->hitbox;
        if (playerState.slowFall) {
            playerState.velocity.y += (gravity / 4) * dt;
        }
        else {
            playerState.velocity.y += gravity * dt;
        }

        r32 dx = playerState.velocity.x * dt;

        HitResult hit{};
        Collision::SweepBoxHorizontal(&pCurrentLevel->tilemap, hitbox, pPlayer->position, dx, hit);
        pPlayer->position.x = hit.location.x;
        if (hit.blockingHit) {
            playerState.velocity.x = 0;
        }

        r32 dy = playerState.velocity.y * dt;
        Collision::SweepBoxVertical(&pCurrentLevel->tilemap, hitbox, pPlayer->position, dy, hit);
        pPlayer->position.y = hit.location.y;
        if (hit.blockingHit) {
            // If ground
            if (playerState.velocity.y > 0) {
                playerState.inAir = false;
                playerState.doubleJumped = false;
            }

            playerState.velocity.y = 0;
        }
        else {
            // TODO: Add coyote time
            playerState.inAir = true;
        }
    }

    static void PlayerShoot(Actor* pPlayer, r64 dt) {
        constexpr r32 shootDelay = 0.16f;

        PlayerState& playerState = pPlayer->playerState;
        if (playerState.shootTimer > dt) {
            playerState.shootTimer -= dt;
        }
        else playerState.shootTimer = 0.0f;

        if (Input::ButtonDown(BUTTON_B) && playerState.shootTimer <= 0.0f) {
            playerState.shootTimer += shootDelay;

            constexpr s32 grenadePresetIndex = 1;
            Actor* arrow = SpawnActor(grenadePresetIndex);
            if (arrow != nullptr) {
                GrenadeState& grenadeState = arrow->grenadeState;
                const Vec2 fwdOffset = Vec2{ 0.375f * playerState.direction, -0.25f };
                const Vec2 upOffset = Vec2{ 0.1875f * playerState.direction, -0.5f };
                const Vec2 downOffset = Vec2{ 0.25f * playerState.direction, -0.125f };

                arrow->position = pPlayer->position;
                grenadeState.velocity = Vec2{};
                //arrow->type = playerState.weapon;
                grenadeState.bounces = 10;

                if (playerState.aMode == AimFwd) {
                    arrow->position = arrow->position + fwdOffset;
                    grenadeState.velocity.x = 40.0f * playerState.direction;
                }
                else {
                    grenadeState.velocity.x = 28.28f * playerState.direction;
                    grenadeState.velocity.y = (playerState.aMode == AimUp) ? -28.28f : 28.28f;
                    arrow->position = arrow->position + ((playerState.aMode == AimUp) ? upOffset : downOffset);
                }

                //if (playerState.weapon == WpnLauncher) {
                grenadeState.velocity = grenadeState.velocity * 0.75f;
                //}
                grenadeState.velocity = grenadeState.velocity + playerState.velocity * dt;
            }
        }
    }

    static void PlayerAnimate(Actor* pPlayer, r64 dt) {
        PlayerState& playerState = pPlayer->playerState;
        // Legs mode
        if (playerState.velocity.y < 0) {
            playerState.lMode = LegsJump;
        }
        else if (playerState.velocity.y > 0) {
            playerState.lMode = LegsFall;
        }
        else if (abs(playerState.velocity.x) > 0) {
            playerState.lMode = LegsFwd;
        }
        else {
            playerState.lMode = LegsIdle;
        }

        // Head mode
        if (playerState.velocity.y > 0 && !playerState.slowFall) {
            playerState.hMode = HeadFall;
        }
        else if (abs(playerState.velocity.x) > 0) {
            playerState.hMode = HeadFwd;
        }
        else {
            playerState.hMode = HeadIdle;
        }

        // Wing mode
        if (playerState.velocity.y < 0) {
            playerState.wMode = WingJump;
        }
        else if (playerState.velocity.y > 0 && !playerState.slowFall) {
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
    }

    static void UpdateActors(Sprite** ppNextSprite, r64 dt) {
        // TODO: Temporarily using a vector for this, change later
        static std::vector<PoolHandle<Actor>> removeList;
        removeList.clear();

        for (u32 i = 0; i < actors.Count(); i++)
        {
            PoolHandle<Actor> handle = actors.GetHandle(i);
            Actor* pActor = actors.Get(handle);

            // Can this ever bet true?
            if (pActor == nullptr) {
                continue;
            }

            switch (pActor->pPrototype->behaviour) {
            case ACTOR_BEHAVIOUR_PLAYER: {
                PlayerState& state = pActor->playerState;
                PlayerInput(pActor, dt);
                PlayerBgCollision(pActor, dt);
                PlayerShoot(pActor, dt);
                PlayerAnimate(pActor, dt);

                if (state.damageTimer > dt) {
                    state.damageTimer -= dt;
                }
                else state.damageTimer = 0.0f;

                DrawPlayer(pActor, ppNextSprite, dt);

                break;
            }
            case ACTOR_BEHAVIOUR_GRENADE: {
                GrenadeState& state = pActor->grenadeState;
                const AABB& hitbox = pActor->pPrototype->hitbox;

                r32 dx = state.velocity.x * dt;

                HitResult hit{};
                Collision::SweepBoxHorizontal(&pCurrentLevel->tilemap, hitbox, pActor->position, dx, hit);
                if (hit.blockingHit) {
                    //if (arrow->type == WpnBow || arrow->bounces == 0) {
                    if (state.bounces == 0) {
                        removeList.push_back(handle);
                        //PoolHandle<Impact> hitHandle = hitPool.Add();
                        //Impact* impact = hitPool[hitHandle];
                        //impact->pos = hit.impactPoint;
                        //impact->accumulator = 0.0f;
                        continue;
                    }
                    pActor->position.x = hit.location.x;
                    state.velocity.x *= -1.0f;
                    state.bounces--;
                }
                else pActor->position.x += dx;

                //if (arrow->type == WpnLauncher) {
                state.velocity.y += dt * gravity * 4.0f;
                //}
                r32 dy = state.velocity.y * dt;

                Collision::SweepBoxVertical(&pCurrentLevel->tilemap, hitbox, pActor->position, dy, hit);
                if (hit.blockingHit) {
                    //if (arrow->type == WpnBow || arrow->bounces == 0) {
                    if (state.bounces == 0) {
                        removeList.push_back(handle);
                        //PoolHandle<Impact> hitHandle = hitPool.Add();
                        //Impact* impact = hitPool[hitHandle];
                        //impact->pos = hit.impactPoint;
                        //impact->accumulator = 0.0f;
                        continue;
                    }
                    pActor->position.y = hit.location.y;
                    state.velocity.y *= -1.0f;
                    //arrow->bounces--;
                }
                else pActor->position.y += dy;

                if (pActor->position.x < viewport.x || pActor->position.x > viewport.x + VIEWPORT_WIDTH_METATILES || pActor->position.y < viewport.y || pActor->position.y > viewport.y + VIEWPORT_HEIGHT_METATILES) {
                    removeList.push_back(handle);
                }

                const Vec2 dir = state.velocity.Normalize();
                const r32 angle = atan2f(dir.y, dir.x);
                const s32 frameIndex = (s32)roundf(((angle + pi) / (pi * 2)) * 7);
                DrawActorFrame(pActor, frameIndex, ppNextSprite);

                break;
            }
            case ACTOR_BEHAVIOUR_ENEMY_SKULL: {
                EnemyState& state = pActor->enemyState;
                if (state.health <= 0) {
                    removeList.push_back(handle);
                    continue;
                }

                static const r32 amplitude = 2.5f;
                r32 sineTime = sin(gameplaySecondsElapsed);
                pActor->position.y = state.baseHeight + sineTime * amplitude;

                // Stupid collision with bullets
                for (int e = 0; e < actors.Count(); e++) {
                    PoolHandle<Actor> otherHandle = actors.GetHandle(e);
                    Actor* pOther = actors.Get(otherHandle);
                    if (pOther == nullptr || pOther->pPrototype->type != ACTOR_TYPE_PROJECTILE_FRIENDLY) {
                        continue;
                    }

                    if (Collision::BoxesOverlap(pActor->pPrototype->hitbox, pActor->position, pOther->pPrototype->hitbox, pOther->position)) {
                        removeList.push_back(otherHandle);

                        /*PoolHandle<Impact> hitHandle = hitPool.Add();
                        Impact* impact = hitPool[hitHandle];
                        impact->pos = arrow->pos;
                        impact->accumulator = 0.0f;*/

                        s32 damage = (rand() % 2) + 1;
                        state.health -= damage;
                        state.damageTimer = damageDelay;

                        // Add damage numbers
                        // Random point inside enemy hitbox
                        /*Vec2 dmgPos = Vec2{(r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.x) + enemyHitboxPos.x - enemyHitboxDimensions.x / 2.0f, (r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.y) + enemyHitboxPos.y - enemyHitboxDimensions.y / 2.0f};

                        PoolHandle<DamageNumber> dmgHandle = damageNumberPool.Add();
                        DamageNumber* dmgNumber = damageNumberPool[dmgHandle];
                        dmgNumber->damage = damage;
                        dmgNumber->pos = dmgPos;
                        dmgNumber->accumulator = 0.0f;*/
                    }
                }

                // Player take damage
                /*if (playerState.damageTimer == 0) {
                    static const Metasprite enemyMetasprite = Metasprite::GetMetaspritesPtr()[5];
                    static const Collision::Collider enemyHitbox = enemyMetasprite.colliders[0];

                    const Vec2 hitboxPos = Vec2{ enemy->pos.x + enemyHitbox.xOffset, enemy->pos.y + enemyHitbox.yOffset };
                    const Vec2 hitboxDimensions = Vec2{ enemyHitbox.width, enemyHitbox.height };

                    const Metasprite characterMetasprite = Metasprite::GetMetaspritesPtr()[0];
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
                }*/

                if (state.damageTimer > dt) {
                    state.damageTimer -= dt;
                }
                else state.damageTimer = 0.0f;

                pActor->drawData.paletteOverride = (state.damageTimer > 0) ? (s32)(gameplaySecondsElapsed * 20) % 4 : -1;
                DrawActorFrame(pActor, 0, ppNextSprite);

                break;
            }
            default: {
                DrawActorFrame(pActor, 0, ppNextSprite);
                break;
            }
            }

            for (auto& handle : removeList) {
                actors.Remove(handle);
            }
        }
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
}