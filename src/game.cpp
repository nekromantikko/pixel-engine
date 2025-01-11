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

namespace Game {
    r64 secondsElapsed = 0.0f;
    r64 averageFramerate;
    // Number of successive frame times used for average framerate calculation
#define SUCCESSIVE_FRAME_TIME_COUNT 64
    r64 successiveFrameTimes[SUCCESSIVE_FRAME_TIME_COUNT]{0};

    // Settings
    Rendering::Settings* pRenderSettings;

    // Viewport
    Viewport viewport;

    Level level;
#define HUD_TILE_COUNT 128

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

    Rendering::ChrSheet playerBank;

    bool paused = false;

    Rendering::Sprite* pSprites;
    Rendering::ChrSheet* pChr;
    Rendering::Nametable* pNametables;

    static void InitializeLevel() {
        LoadLevel(&level, "assets/test.lev");

        playerState.x = 0;
        playerState.y = 0;
        playerState.direction = DirRight;
        playerState.weapon = WpnLauncher;

        // Find first player spawn
        for (u32 i = 0; i < level.screenCount; i++) {
            const Screen& screen = level.screens[i];
            for (u32 t = 0; t < LEVEL_SCREEN_WIDTH_METATILES * LEVEL_SCREEN_HEIGHT_METATILES; t++) {
                const LevelTile& tile = screen.tiles[t];

                if (tile.actorType == ACTOR_PLAYER_START) {
                    const Vec2 screenRelativePos = TileIndexToScreenOffset(t);
                    const Vec2 worldPos = ScreenOffsetToWorld(&level, screenRelativePos, i);

                    playerState.x = worldPos.x + 1.0f;
                    playerState.y = worldPos.y;

                    break;
                }
            }
        }
    }

	void Initialize(Rendering::RenderContext* pRenderContext) {
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.w = VIEWPORT_WIDTH_TILES;
        viewport.h = VIEWPORT_HEIGHT_TILES;

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

        Tileset::LoadTileset("assets/forest.til");
        Metasprite::LoadMetasprites("assets/meta.spr");

        InitializeLevel();

        pNametables = Rendering::GetNametablePtr(pRenderContext, 0);
        RefreshViewport(&viewport, pNametables, &level);

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
        snprintf(hudText, 64, " %4d FPS (%2d ms) (%04d, %04d) %s", (int)(averageFramerate), (int)(dt*1000), (int)viewport.x, (int)viewport.y, level.name);
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

    void DrawPlayer(Rendering::RenderContext* pRenderContext, Rendering::Sprite** ppNextSprite, r64 dt) {
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

    void Render(Rendering::RenderContext* pRenderContext, r64 dt) {
        Rendering::Util::ClearSprites(pSprites, 256);
        Rendering::Sprite* pNextSprite = pSprites;
        
        DrawDamageNumbers(&pNextSprite);
        DrawShield(&pNextSprite, dt);
        DrawPlayer(pRenderContext, &pNextSprite, dt);
        DrawArrows(&pNextSprite);
        DrawHits(&pNextSprite);

        // Draw enemy
        Metasprite::Metasprite enemyMetasprite = Metasprite::GetMetaspritesPtr()[5];
        IVec2 enemyPixelPos = WorldPosToScreenPixels(enemyPos);
        Rendering::Util::CopyMetasprite(enemyMetasprite.spritesRelativePos, pNextSprite, enemyMetasprite.spriteCount, enemyPixelPos, false, false);
        pNextSprite += enemyMetasprite.spriteCount;

        UpdateHUD(pRenderContext, dt);
        RenderHUD(pRenderContext);

        /*for (int i = 0; i < 272; i++) {
            float sine = sin(secondsElapsed + (i / 8.0f));
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
        Rendering::SetRenderState(pRenderContext, 16, 272, state);
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

        if (Input::Pressed(Input::A)) {
            playerState.vSpeed = -31.25f;
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
        Collision::SweepBoxHorizontal(&level, hitboxPos, hitboxDimensions, dx, hit);
        playerState.x = hit.location.x;
        if (hit.blockingHit) {
            playerState.hSpeed = 0;
        }

        r32 dy = playerState.vSpeed * dt;
        hitboxPos = Vec2{ playerState.x + hitbox.xOffset, playerState.y + hitbox.yOffset };
        Collision::SweepBoxVertical(&level, hitboxPos, hitboxDimensions, dy, hit);
        playerState.y = hit.location.y;
        if (hit.blockingHit) {
            playerState.vSpeed = 0;
        }
    }

    void PlayerShoot(r64 dt) {
        if (shootTimer > dt) {
            shootTimer -= dt;
        }
        else shootTimer = 0.0f;

        if (Input::Down(Input::B) && shootTimer < shootDelay) {
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

    constexpr Vec2 viewportScrollThreshold = { 8.0f, 6.0f };

    void UpdateViewport() {
        Vec2 viewportCenter = Vec2{ viewport.x + viewport.w / 2.0f, viewport.y + viewport.h / 2.0f };
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

        MoveViewport(&viewport, pNametables, &level, delta.x, delta.y);
    }

    void Step(r64 dt, Rendering::RenderContext* pRenderContext) {
        secondsElapsed += dt;
        averageFramerate = GetAverageFramerate(dt);

        // TODO: Move out of game logic
        Input::Poll();

        if (!paused) {
            PlayerInput(dt);
            PlayerBgCollision(dt, pRenderContext);
            PlayerShoot(dt);
            PlayerAnimate(dt);

            Metasprite::Metasprite enemyMetasprite = Metasprite::GetMetaspritesPtr()[5];
            Collision::Collider enemyHitbox = enemyMetasprite.colliders[0];
            Vec2 enemyHitboxPos = Vec2{ enemyPos.x + enemyHitbox.xOffset, enemyPos.y + enemyHitbox.yOffset };
            Vec2 enemyHitboxDimensions = Vec2{ enemyHitbox.width, enemyHitbox.height };

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
                Collision::SweepBoxHorizontal(&level, hitboxPos, hitboxDimensions, dx, hit);
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
                Collision::SweepBoxVertical(&level, hitboxPos, hitboxDimensions, dy, hit);
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

                // Collision with enemy
                hitboxPos = Vec2{ arrow->pos.x + hitbox.xOffset * (hFlip ? -1.0f : 1.0f), arrow->pos.y + hitbox.yOffset * (vFlip ? -1.0f : 1.0f) };
                if (hitboxPos.x - hitboxDimensions.x / 2.0f < enemyHitboxPos.x + enemyHitboxDimensions.x / 2.0f &&
                    hitboxPos.x + hitboxDimensions.x / 2.0f > enemyHitboxPos.x - enemyHitboxDimensions.x / 2.0f &&
                    hitboxPos.y - hitboxDimensions.y / 2.0f < enemyHitboxPos.y + enemyHitboxDimensions.y / 2.0f &&
                    hitboxPos.y + hitboxDimensions.y / 2.0f > enemyHitboxPos.y - enemyHitboxDimensions.y / 2.0f) {

                    arrowPool.Remove(handle);
                    PoolHandle<Impact> hitHandle = hitPool.Add();
                    Impact* impact = hitPool[hitHandle];
                    impact->pos = arrow->pos;
                    impact->accumulator = 0.0f;

                    // Add damage numbers
                    s32 damage = (rand() % 10) + 5;
                    // Random point inside enemy hitbox
                    Vec2 dmgPos = Vec2{ (r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.x) + enemyHitboxPos.x - enemyHitboxDimensions.x / 2.0f, (r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.y) + enemyHitboxPos.y - enemyHitboxDimensions.y / 2.0f };

                    PoolHandle<DamageNumber> dmgHandle = damageNumberPool.Add();
                    DamageNumber* dmgNumber = damageNumberPool[dmgHandle];
                    dmgNumber->damage = damage;
                    dmgNumber->pos = dmgPos;
                    dmgNumber->accumulator = 0.0f;

                    continue;
                }

                if (arrow->pos.x < viewport.x || arrow->pos.x > viewport.x + viewport.w || arrow->pos.y < viewport.y || arrow->pos.y > viewport.y + viewport.h) {
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

            // Update enemy pos
            const r32 yMid = 27.f;
            const r32 amplitude = 7.5f;
            r32 sineTime = sin(secondsElapsed);
            enemyPos.y = yMid + sineTime * amplitude;
            
            UpdateViewport();
        }


        Render(pRenderContext, dt);

        // Corrupt CHR mem
        //int randomInt = rand();
        //Rendering::WriteChrMemory(pContext, sizeof(int), rand() % (CHR_MEMORY_SIZE - sizeof(int)), (u8*)&randomInt);

        // Animate palette
        /*static float paletteAccumulator = 0;
        static int a = 0;
        paletteAccumulator += dt;
        if (paletteAccumulator > 0.1f) {
            a++;
            paletteAccumulator -= 0.1f;
            u8 colors[8] = {
                a % 64,
                (a + 1) % 64,
                (a + 2) % 64,
                (a + 3) % 64,
                (a + 4) % 64,
                (a + 5) % 64,
                (a + 6) % 64,
                (a + 7) % 64,
            };
            Rendering::WritePaletteColors(pContext, 4, 8, 0, colors);
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
        return &level;
    }
}