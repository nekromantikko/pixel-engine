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

    // Input
    Input::ControllerState controllerState;
    Input::ControllerState controllerStatePrev;

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

    struct PlayerState {
        r32 x, y;
        r32 hSpeed, vSpeed;
        HeadMode hMode;
        LegsMode lMode;
        WingMode wMode;
        r32 wingCounter;
        u32 wingFrame;
        s32 vOffset;
    };


    PlayerState playerState{};
    r32 gravity = 70;

    Rendering::CHRSheet chrSheet;

	void Initialize(Rendering::RenderContext* pRenderContext) {
        viewport.x = 0.0f;
        viewport.y = 12.0f;
        viewport.w = VIEWPORT_WIDTH_TILES;
        viewport.h = VIEWPORT_HEIGHT_TILES;

        // Init chr memory
        Rendering::Util::CreateChrSheet("chr000.bmp", &chrSheet);
        Rendering::Util::WriteChrTiles(pRenderContext, 0, 256, 0, 0, &chrSheet);
        Rendering::Util::CreateChrSheet("chr001.bmp", &chrSheet);
        Rendering::Util::WriteChrTiles(pRenderContext, 1, 256, 0, 0, &chrSheet);
        Rendering::Util::CreateChrSheet("wings.bmp", &chrSheet);

        playerState.x = 30;
        playerState.y = 16;

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

    u32 DrawDebugCharacter(Rendering::RenderContext* pRenderContext, u32 spriteOffset, s32 x, s32 y, bool flip) {
        // Animate chr sheet
        switch (playerState.hMode) {
        case HeadIdle:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 4, 0x20, 8, &chrSheet);
            break;
        case HeadFwd:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 4, 0x24, 8, &chrSheet);
            break;
        case HeadFall:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 4, 0x28, 8, &chrSheet);
            break;
        default:
            break;
        }

        switch (playerState.lMode) {
        case LegsIdle:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 4, 0x30, 0x0C, &chrSheet);
            break;
        case LegsFwd:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 4, 0x34, 0x0C, &chrSheet);
            break;
        case LegsJump:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 4, 0x38, 0x0C, &chrSheet);
            break;
        case LegsFall:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 4, 0x3C, 0x0C, &chrSheet);
            break;
        default:
            break;
        }

        switch (playerState.wMode) {
        case WingFlap:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 8, 8 * playerState.wingFrame, 0, &chrSheet);
            break;
        case WingJump:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 8, 0x10, 0, &chrSheet);
            break;
        case WingFall:
            Rendering::Util::WriteChrTiles(pRenderContext, 1, 8, 0, 0, &chrSheet);
            break;
        default:
            break;
        }

        Metasprite::Metasprite characterMetasprite = Metasprite::GetMetaspritesPtr()[0];
        Rendering::Util::WriteMetasprite(pRenderContext, characterMetasprite.spritesRelativePos, characterMetasprite.spriteCount, spriteOffset, x, y + playerState.vOffset, flip);
        return characterMetasprite.spriteCount;
    }

    void Render(Rendering::RenderContext* pRenderContext, float dt) {
        Rendering::SetCurrentTime(pRenderContext, secondsElapsed);

        Rendering::ClearSprites(pRenderContext, 0, 256);

        u32 spriteOffset = 0;
        /*for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                spriteOffset += DrawDebugCharacter(pContext, spriteOffset, xCharacter + x * 32, yCharacter + y * 32, secondsElapsed + x*0.1 + y*0.8);
            }
        }*/
        spriteOffset += DrawDebugCharacter(pRenderContext, spriteOffset, (s32)((playerState.x - viewport.x) * TILE_SIZE), (s32)((playerState.y - viewport.y) * TILE_SIZE), flipCharacter);

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

        // Debug collision things
        ImGuiIO& io = ImGui::GetIO();

        Vec2 lineOrigin = { playerState.x, playerState.y };
        r32 renderScale = (1920.0f / 512.0f) * TILE_SIZE;
        Vec2 lineEnd = { io.MousePos.x / renderScale + viewport.x, io.MousePos.y / renderScale + viewport.y };
        Vec2 lineDir = (lineEnd - lineOrigin);
        r32 length = lineDir.Length();
        lineDir = lineDir.Normalize();

        // DEBUG_LOG("%f, %f\n", (r32)lineDir.x, (r32)lineDir.y);

        Collision::HitResult hit{};
        Collision::RaycastTilesWorldSpace(pRenderContext, lineOrigin, lineDir, length, hit);

        Rendering::Sprite debugSprite = {
            (s32)round((lineOrigin.y - viewport.y - 0.5f) * TILE_SIZE),
            (s32)round((lineOrigin.x - viewport.x - 0.5f) * TILE_SIZE),
            0x80,
            hit.blockingHit ? 1 : 0
        };
        // Rendering::WriteSprites(pRenderContext, 1, spriteOffset++, &debugSprite);

        debugSprite.y = (s32)round((lineEnd.y - viewport.y - 0.5f) * TILE_SIZE);
        debugSprite.x = (s32)round((lineEnd.x - viewport.x - 0.5f) * TILE_SIZE);
        Rendering::WriteSprites(pRenderContext, 1, spriteOffset++, &debugSprite);

        debugSprite.y = (s32)round((hit.impactPoint.y - viewport.y - 0.5f) * TILE_SIZE);
        debugSprite.x = (s32)round((hit.impactPoint.x - viewport.x - 0.5f) * TILE_SIZE);
        Rendering::WriteSprites(pRenderContext, 1, spriteOffset++, &debugSprite);

        // Visualize impact normal
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImVec2 drawLineOrigin = ImVec2((hit.impactPoint.x - viewport.x) * renderScale, (hit.impactPoint.y - viewport.y) * renderScale);
        ImVec2 drawLineEnd = ImVec2((hit.impactPoint.x + hit.impactNormal.x - viewport.x) * renderScale, (hit.impactPoint.y + hit.impactNormal.y - viewport.y) * renderScale);
        drawList->AddCircle(drawLineOrigin, 8, IM_COL32(255, 0, 0, 255));
        drawList->AddLine(drawLineOrigin, drawLineEnd, IM_COL32(255, 0, 0, 255));

        ImGui::Render();
        Rendering::Render(pRenderContext);
    }

    void Step(float dt, Rendering::RenderContext* pRenderContext) {
        secondsElapsed += dt;

        // Poll input
        controllerState = Input::PollInput(controllerState);

        // LevelEditor::HandleInput(&editorState, controllerState, controllerStatePrev, &viewport, dt);

        if (controllerState & Input::ControllerState::Left) {
            flipCharacter = true;
            playerState.hSpeed = -12.5f;
        }
        else if (controllerState & Input::ControllerState::Right) {
            flipCharacter = false;
            playerState.hSpeed = 12.5f;
        }
        else {
            playerState.hSpeed = 0;
        }

        if ((controllerState & Input::ControllerState::Start) && !(controllerStatePrev & Input::ControllerState::Start)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if ((controllerState & Input::ControllerState::A) && !(controllerStatePrev & Input::ControllerState::A)) {
            playerState.vSpeed = -31.25f;
        }

        if (playerState.vSpeed < 0 && !(controllerState & Input::ControllerState::A) && (controllerStatePrev & Input::ControllerState::A)) {
            playerState.vSpeed /= 2;
        }

        bool slowFall = true;
        if (!(controllerState & Input::ControllerState::A) || playerState.vSpeed < 0) {
            slowFall = false;
        }
 
        if (slowFall) {
            playerState.vSpeed += (gravity / 4) * dt;
        }
        else {
            playerState.vSpeed += gravity * dt;
        }

        r32 xPlayerCollision = playerState.x;
        r32 yPlayerCollision = playerState.y + 2;

        Collision::TileCollision* bgCollision = Collision::GetBgCollisionPtr();
        if (playerState.vSpeed > 0 && yPlayerCollision <= NAMETABLE_HEIGHT_TILES) {
            u32 screenIndex = (u32)xPlayerCollision / NAMETABLE_WIDTH_TILES;
            r32 screenRelativeX = xPlayerCollision - screenIndex * NAMETABLE_WIDTH_TILES;
            u32 nametableIndex = screenIndex % NAMETABLE_COUNT;
            u32 yTile = (u32)yPlayerCollision;
            u32 xTile = (u32)screenRelativeX;
            u32 tileIndex = yTile * NAMETABLE_WIDTH_TILES + xTile;
            u8 collidingTile;
            Rendering::ReadNametable(pRenderContext, nametableIndex, 1, tileIndex, &collidingTile);
            if (bgCollision[collidingTile].type == Collision::TileSolid) {
                playerState.vSpeed = 0;
                playerState.y = floor(playerState.y);
            }
        }
        playerState.y += playerState.vSpeed * dt;
        playerState.x += playerState.hSpeed * dt;

        controllerStatePrev = controllerState;

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
        if (playerState.vSpeed > 0 && !slowFall) {
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
        else if (playerState.vSpeed > 0 && !slowFall) {
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