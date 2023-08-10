#include "game.h"
#include "system.h"
#include "input.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "rendering_util.h"
#include "level.h"
#include "leveleditor.h"
#include "viewport.h"
#include "collision.h"
#include "metasprite.h"
#include "memory_pool.h"
#include "editor_core.h"
#include "editor_debug.h"
#include "editor_sprites.h"
#include "editor_chr.h"
#include "editor_tiles.h"
#include <imgui.h>

namespace Game {
    r32 secondsElapsed = 0.0f;

    // Settings
    Rendering::Settings* pRenderSettings;

    // Editor
    LevelEditor::EditorState editorState;
    Editor::EditorContext* pEditorContext;

    // Viewport
    Viewport viewport;

    Level level;
#define HUD_TILE_COUNT 128

    // Sprite stufff
    bool flipCharacter = false;
    
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

    struct PlayerState {
        r32 x, y;
        r32 hSpeed, vSpeed;
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
    };

    MemoryPool<Arrow> arrowPool;

    constexpr r32 shootDelay = 0.16f;
    r32 shootTimer = 0;

    constexpr u32 impactFrameCount = 4;
    constexpr r32 impactAnimLength = 0.16f;

    struct Impact {
        Vec2 pos;
        r32 accumulator;
    };
    MemoryPool<Impact> hitPool;

    Vec2 enemyPos = Vec2{ 48.0f, 27.0f };

    constexpr r32 damageNumberLifetime = 1.0f;

    struct DamageNumber {
        Vec2 pos;
        s32 damage;
        r32 accumulator;
    };
    MemoryPool<DamageNumber> damageNumberPool;

    Rendering::CHRSheet playerBank;

	void Initialize(Rendering::RenderContext* pRenderContext) {
        viewport.x = 0.0f;
        viewport.y = 12.0f;
        viewport.w = VIEWPORT_WIDTH_TILES;
        viewport.h = VIEWPORT_HEIGHT_TILES;

        // Init chr memory
        // TODO: Pre-process these instead of loading from bitmap at runtime!
        Rendering::CHRSheet temp;
        Rendering::Util::CreateChrSheet("chr000.bmp", &temp);
        Rendering::Util::WriteChrTiles(pRenderContext, 0, 256, 0, 0, &temp);
        Rendering::Util::CreateChrSheet("chr001.bmp", &temp);
        Rendering::Util::WriteChrTiles(pRenderContext, 1, 256, 0, 0, &temp);

        Rendering::Util::CreateChrSheet("player.bmp", &playerBank);

        playerState.x = 30;
        playerState.y = 16;

        arrowPool.Init(512);
        hitPool.Init(512);
        damageNumberPool.Init(512);

        editorState.pLevel = &level;
        editorState.pRenderContext = pRenderContext;

        Collision::LoadBgCollision("bg.til");
        Metasprite::LoadMetasprites("meta.spr");
        LoadLevel(&level, "test.lev");

        // Render all of first and second nametable
        for (int i = 0; i < NAMETABLE_COUNT; i++) {
            Rendering::WriteNametable(pRenderContext, i, NAMETABLE_SIZE - HUD_TILE_COUNT, HUD_TILE_COUNT, (u8*)&level.screens[i] + HUD_TILE_COUNT);
        }

        // SETTINGS
        pRenderSettings = Rendering::GetSettingsPtr(pRenderContext);

        // EDITOR
        pEditorContext = Editor::CreateEditorContext(pRenderContext);
	}

    void Free() {
        Editor::FreeEditorContext(pEditorContext);
        arrowPool.Free();
        hitPool.Free();
        damageNumberPool.Free();
    }

    void UpdateHUD(Rendering::RenderContext* pContext, float dt) {
        float fps = 1.0f / dt;
        char hudText[128];
        snprintf(hudText, 64, " %4d FPS (%2d ms) (%04d, %04d) %s", (int)(fps), (int)(dt*1000), (int)viewport.x, (int)viewport.y, level.name);
        // snprintf(hudText + 64, 64, " Editor mode: %s, Palette: %#x ", LevelEditor::GetEditorModeName(editorState.mode), editorState.palette);
        Rendering::WriteNametable(pContext, 0, 128, 0, (u8*)hudText);
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
    constexpr u8 playerBowFrameBankOffsets[3] = { 0x60, 0x68, 0x70 };

    constexpr u8 playerWingFrameChrOffset = 0x00;
    constexpr u8 playerHeadFrameChrOffset = 0x08;
    constexpr u8 playerLegsFrameChrOffset = 0x0C;
    constexpr u8 playerBowFrameChrOffset = 0x10;

    constexpr u8 playerWingFrameTileCount = 8;
    constexpr u8 playerHeadFrameTileCount = 4;
    constexpr u8 playerLegsFrameTileCount = 4;
    constexpr u8 playerBowFrameTileCount = 8;

    IVec2 WorldPosToScreenPixels(Vec2 pos) {
        return IVec2{
            (s32)round((pos.x - viewport.x) * TILE_SIZE),
            (s32)round((pos.y - viewport.y) * TILE_SIZE)
        };
    }

    void DrawPlayer(Rendering::RenderContext* pRenderContext, u32& spriteOffset, IVec2 pos, bool flip) {
        // Animate chr sheet using player bank
        // TODO: Maybe do this on the GPU?
        Rendering::Util::WriteChrTiles(
            pRenderContext, 
            spriteChrSheetIndex,
            playerBowFrameTileCount, 
            playerBowFrameBankOffsets[playerState.aMode], 
            playerBowFrameChrOffset, 
            &playerBank
        );
        Rendering::Util::WriteChrTiles(
            pRenderContext, 
            spriteChrSheetIndex,
            playerHeadFrameTileCount, 
            playerHeadFrameBankOffsets[playerState.aMode * 3 + playerState.hMode], 
            playerHeadFrameChrOffset, 
            &playerBank
        );
        Rendering::Util::WriteChrTiles(
            pRenderContext,
            spriteChrSheetIndex,
            playerLegsFrameTileCount,
            playerLegsFrameBankOffsets[playerState.lMode],
            playerLegsFrameChrOffset,
            &playerBank
        );
        u8 wingFrame = (playerState.wMode == WingFlap) ? playerState.wingFrame : (playerState.wMode == WingJump ? 2 : 0);
        Rendering::Util::WriteChrTiles(
            pRenderContext,
            spriteChrSheetIndex,
            playerWingFrameTileCount,
            playerWingFrameBankOffsets[wingFrame],
            playerWingFrameChrOffset,
            &playerBank
        );

        Metasprite::Metasprite characterMetasprite = Metasprite::GetMetaspritesPtr()[playerState.aMode];
        Rendering::Util::WriteMetasprite(pRenderContext, characterMetasprite.spritesRelativePos, characterMetasprite.spriteCount, spriteOffset, IVec2{ pos.x, pos.y + playerState.vOffset }, flip, false);
        spriteOffset += characterMetasprite.spriteCount;
    }

    void DrawDamageNumbers(Rendering::RenderContext* pRenderContext, u32& spriteOffset) {
        char dmgStr[8]{};

        for (int i = 0; i < damageNumberPool.Count(); i++) {
            DamageNumber& dmg = damageNumberPool[i];

            _itoa_s(dmg.damage, dmgStr, 10);

            // Ascii character '0' = 0x30
            s32 chrOffset = 0x70 - 0x30;

            for (int i = 0; i < strlen(dmgStr); i++) {
                IVec2 pixelPos = WorldPosToScreenPixels(dmg.pos);
                Rendering::Sprite sprite = {
                    pixelPos.y,
                    pixelPos.x + i * 5,
                    dmgStr[i] + chrOffset,
                    1
                };
                Rendering::WriteSprites(pRenderContext, 1, spriteOffset++, &sprite);
            }
        }
    }

    void DrawArrows(Rendering::RenderContext* pRenderContext, u32& spriteOffset) {
        for (int i = 0; i < arrowPool.Count(); i++) {
            Arrow& arrow = arrowPool[i];

            bool hFlip = arrow.vel.x < 0.0f;
            bool vFlip = arrow.vel.y < -10.0f;

            u32 spriteIndex = abs(arrow.vel.y) < 10.0f ? 3 : 4;
            Metasprite::Metasprite metasprite = Metasprite::GetMetaspritesPtr()[spriteIndex];
            IVec2 pixelPos = WorldPosToScreenPixels(arrow.pos);
            Rendering::Util::WriteMetasprite(pRenderContext, metasprite.spritesRelativePos, metasprite.spriteCount, spriteOffset, pixelPos, hFlip, vFlip);
            spriteOffset += metasprite.spriteCount;
        }
    }

    void DrawHits(Rendering::RenderContext* pRenderContext, u32& spriteOffset) {
        for (int i = 0; i < hitPool.Count(); i++) {
            Impact& impact = hitPool[i];

            u32 frame = (u32)((impact.accumulator / impactAnimLength) * impactFrameCount);
            IVec2 pixelPos = WorldPosToScreenPixels(impact.pos);

            Rendering::Sprite debugSprite = {
                pixelPos.y - (TILE_SIZE / 2),
                pixelPos.x - (TILE_SIZE / 2),
                0x20 + frame,
                1
            };
            Rendering::WriteSprites(pRenderContext, 1, spriteOffset++, &debugSprite);
        }
    }

    void Render(Rendering::RenderContext* pRenderContext, float dt) {
        Rendering::SetCurrentTime(pRenderContext, secondsElapsed);

        Rendering::ClearSprites(pRenderContext, 0, 256);

        u32 spriteOffset = 0;
        
        DrawDamageNumbers(pRenderContext, spriteOffset);
        IVec2 playerPixelPos = WorldPosToScreenPixels(Vec2{ playerState.x, playerState.y });
        DrawPlayer(pRenderContext, spriteOffset, playerPixelPos, flipCharacter);
        DrawArrows(pRenderContext, spriteOffset);
        DrawHits(pRenderContext, spriteOffset);

        // Draw enemy
        Metasprite::Metasprite enemyMetasprite = Metasprite::GetMetaspritesPtr()[5];
        IVec2 enemyPixelPos = WorldPosToScreenPixels(enemyPos);
        Rendering::Util::WriteMetasprite(pRenderContext, enemyMetasprite.spritesRelativePos, enemyMetasprite.spriteCount, spriteOffset, enemyPixelPos, false, false);
        spriteOffset += enemyMetasprite.spriteCount;

        // LevelEditor::DrawSelection(&editorState, &viewport, spriteOffset);

        UpdateHUD(pRenderContext, dt);
        RenderHUD(pRenderContext);

        /*for (int i = 0; i < 272; i++) {
            float sine = sin(secondsElapsed + (i / 8.0f));
            Rendering::RenderState state = {
                viewport.x + (sine * 8),
                viewport.y,
                0,
                mode == Scroll ? 1 : 0
            };
            Rendering::SetRenderState(pContext, 16 + i, 1, state);
        }*/

        Rendering::RenderState state = {
            (s32)(viewport.x * TILE_SIZE),
            (s32)(viewport.y* TILE_SIZE)
        };
        Rendering::SetRenderState(pRenderContext, 16, 272, state);

        // GUI
        Rendering::BeginImGuiFrame(pRenderContext);
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        Editor::Debug::DrawDebugWindow(pEditorContext, pRenderContext);

        Editor::Sprites::DrawPreviewWindow(pEditorContext);
        Editor::Sprites::DrawMetaspriteWindow(pEditorContext);
        Editor::Sprites::DrawSpriteEditor(pEditorContext);

        Editor::CHR::DrawCHRWindow(pEditorContext);
        Editor::Tiles::DrawBgCollisionWindow(pEditorContext);
        Editor::Tiles::DrawCollisionEditor(pEditorContext, pRenderContext);

        ImGui::Render();
        Rendering::Render(pRenderContext);
    }

    void PlayerInput(r32 dt) {
        if (Input::Down(Input::DPadLeft)) {
            flipCharacter = true;
            playerState.hSpeed = -12.5f;
        }
        else if (Input::Down(Input::DPadRight)) {
            flipCharacter = false;
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

    }

    void PlayerBgCollision(r32 dt, Rendering::RenderContext* pRenderContext) {
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
        Collision::SweepBoxHorizontal(pRenderContext, hitboxPos, hitboxDimensions, dx, hit);
        playerState.x = hit.location.x;
        if (hit.blockingHit) {
            playerState.hSpeed = 0;
        }

        r32 dy = playerState.vSpeed * dt;
        hitboxPos = Vec2{ playerState.x + hitbox.xOffset, playerState.y + hitbox.yOffset };
        Collision::SweepBoxVertical(pRenderContext, hitboxPos, hitboxDimensions, dy, hit);
        playerState.y = hit.location.y;
        if (hit.blockingHit) {
            playerState.vSpeed = 0;
        }
    }

    void PlayerShoot(r32 dt) {
        if (shootTimer > dt) {
            shootTimer -= dt;
        }
        else shootTimer = 0.0f;

        if (Input::Down(Input::B) && shootTimer < shootDelay) {
            shootTimer += shootDelay;

            Arrow* arrow = arrowPool.AllocObject();
            if (arrow != nullptr) {
                r32 xDir = flipCharacter ? -1.0f : 1.0f;

                const Vec2 fwdOffset = Vec2{ 0.75f * xDir, -0.5f };
                const Vec2 upOffset = Vec2{ 0.375f * xDir, -1.0f };
                const Vec2 downOffset = Vec2{ 0.5f * xDir, -0.25f };

                arrow->pos = Vec2{ playerState.x, playerState.y };
                arrow->vel = Vec2{};

                if (playerState.aMode == AimFwd) {
                    arrow->pos = arrow->pos + fwdOffset;
                    arrow->vel.x = 80.0f * xDir;
                }
                else {
                    arrow->vel.x = 56.56f * xDir;
                    arrow->vel.y = (playerState.aMode == AimUp) ? -56.56f : 56.56f;
                    arrow->pos = arrow->pos + ((playerState.aMode == AimUp) ? upOffset : downOffset);
                }

                arrow->vel = arrow->vel + Vec2{ playerState.hSpeed, playerState.vSpeed } *dt;
            }
        }
    }

    void PlayerAnimate(r32 dt) {
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
            if (playerState.wMode == WingJump) {
                playerState.wingFrame = 2;
                playerState.wingCounter = 0.0f;
            }
            else if (playerState.wMode == WingFall) {
                playerState.wingFrame = 0;
                playerState.wingCounter = 0.0f;
            }
            playerState.wMode = WingFlap;
        }

        // Wing flapping
        playerState.wingCounter += dt / 0.18f;
        while (playerState.wingCounter > 1.0f) {
            playerState.wingFrame++;
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

    void UpdateViewport(Rendering::RenderContext* pRenderContext) {
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

        MoveViewport(&viewport, pRenderContext, &level, delta.x, delta.y);
    }

    void Step(r32 dt, Rendering::RenderContext* pRenderContext) {
        secondsElapsed += dt;

        Input::Poll();

        // LevelEditor::HandleInput(&editorState, controllerState, controllerStatePrev, &viewport, dt);

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
            Arrow& arrow = arrowPool[i];
            //arrow.pos = arrow.pos + (arrow.vel * dt);

            // TODO: Object struct and updater that does this for everything?
            bool hFlip = arrow.vel.x < 0.0f;
            bool vFlip = arrow.vel.y < -10.0f;

            u32 spriteIndex = abs(arrow.vel.y) < 10.0f ? 3 : 4;
            Metasprite::Metasprite metasprite = Metasprite::GetMetaspritesPtr()[spriteIndex];

            Collision::Collider hitbox = metasprite.colliders[0];
            Vec2 hitboxPos = Vec2{ arrow.pos.x + hitbox.xOffset * (hFlip ? -1.0f : 1.0f), arrow.pos.y + hitbox.yOffset * (vFlip ? -1.0f : 1.0f) };
            Vec2 hitboxDimensions = Vec2{ hitbox.width, hitbox.height };

            r32 dx = arrow.vel.x * dt;

            Collision::HitResult hit{};
            /*Collision::SweepBoxHorizontal(pRenderContext, hitboxPos, hitboxDimensions, dx, hit);
            if (hit.blockingHit) {
                arrowPool.FreeObject(i);
                Impact* impact = hitPool.AllocObject();
                impact->pos = hit.impactPoint;
                impact->accumulator = 0.0f;
                continue;
            }
            arrow.pos.x += dx;

            r32 dy = arrow.vel.y * dt;
            hitboxPos = Vec2{ arrow.pos.x + hitbox.xOffset * (hFlip ? -1.0f : 1.0f), arrow.pos.y + hitbox.yOffset * (vFlip ? -1.0f : 1.0f) };
            Collision::SweepBoxVertical(pRenderContext, hitboxPos, hitboxDimensions, dy, hit);
            if (hit.blockingHit) {
                arrowPool.FreeObject(i);
                Impact* impact = hitPool.AllocObject();
                impact->pos = hit.impactPoint;
                impact->accumulator = 0.0f;
                continue;
            }
            arrow.pos.y += dy;*/

            Vec2 deltaPos = arrow.vel * dt;
            Vec2 dir = deltaPos.Normalize();
            Collision::RaycastTilesWorldSpace(pRenderContext, arrow.pos + dir, dir, deltaPos.Length(), hit);
            if (hit.blockingHit) {
                arrowPool.FreeObject(i);
                Impact* impact = hitPool.AllocObject();
                impact->pos = hit.impactPoint;
                impact->accumulator = 0.0f;
                continue;
            }

            arrow.pos = arrow.pos + (arrow.vel * dt);

            // Collision with enemy
            hitboxPos = Vec2{ arrow.pos.x + hitbox.xOffset * (hFlip ? -1.0f : 1.0f), arrow.pos.y + hitbox.yOffset * (vFlip ? -1.0f : 1.0f) };
            if (hitboxPos.x - hitboxDimensions.x / 2.0f < enemyHitboxPos.x + enemyHitboxDimensions.x / 2.0f &&
                hitboxPos.x + hitboxDimensions.x / 2.0f > enemyHitboxPos.x - enemyHitboxDimensions.x / 2.0f &&
                hitboxPos.y - hitboxDimensions.y / 2.0f < enemyHitboxPos.y + enemyHitboxDimensions.y / 2.0f &&
                hitboxPos.y + hitboxDimensions.y / 2.0f > enemyHitboxPos.y - enemyHitboxDimensions.y / 2.0f) {

                arrowPool.FreeObject(i);
                Impact* impact = hitPool.AllocObject();
                impact->pos = arrow.pos;
                impact->accumulator = 0.0f;

                // Add damage numbers
                s32 damage = (rand() % 10) + 5;
                // Random point inside enemy hitbox
                Vec2 dmgPos = Vec2{ (r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.x) + enemyHitboxPos.x - enemyHitboxDimensions.x / 2.0f, (r32)rand() / (r32)(RAND_MAX / enemyHitboxDimensions.y) + enemyHitboxPos.y - enemyHitboxDimensions.y / 2.0f };

                DamageNumber* dmgNumber = damageNumberPool.AllocObject();
                dmgNumber->damage = damage;
                dmgNumber->pos = dmgPos;
                dmgNumber->accumulator = 0.0f;

                continue;
            }

            if (arrow.pos.x < viewport.x || arrow.pos.x > viewport.x + viewport.w || arrow.pos.y < viewport.y || arrow.pos.y > viewport.y + viewport.h) {
                arrowPool.FreeObject(i);
            }
        }

        // Update explosions
        for (int i = 0; i < hitPool.Count(); i++) {
            Impact& impact = hitPool[i];
            impact.accumulator += dt;

            if (impact.accumulator >= impactAnimLength) {
                hitPool.FreeObject(i);
                continue;
            }
        }

        // Update damage numbers
        r32 dmgNumberVel = -3.0f;
        for (int i = 0; i < damageNumberPool.Count(); i++) {
            DamageNumber& dmgNumber = damageNumberPool[i];
            dmgNumber.accumulator += dt;

            if (dmgNumber.accumulator >= damageNumberLifetime) {
                damageNumberPool.FreeObject(i);
                continue;
            }

            dmgNumber.pos.y += dmgNumberVel * dt;
        }

        // Update enemy pos
        const r32 yMid = 27.f;
        const r32 amplitude = 7.5f;
        r32 sineTime = sin(secondsElapsed);
        enemyPos.y = yMid + sineTime * amplitude;

        UpdateViewport(pRenderContext);

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
}