#include "game.h"
#include "system.h"
#include "input.h"
#include <math.h>
#include <string.h>
#include "rendering_util.h"
#include "level.h"
#include "viewport.h"
#include "collision.h"
#include "editor_core.h"
#include "editor_debug.h"
#include "editor_metasprite.h"
#include "editor_chr.h"
#include "editor_collision.h"
#include <imgui.h>

namespace Game {
    r32 secondsElapsed = 0.0f;

    // Settings
    Rendering::Settings* pRenderSettings;

    // Input
    Input::ControllerState controllerState;
    Input::ControllerState controllerStatePrev;

    // Editor
    // Editor::EditorState editorState;
    Editor::EditorContext* pEditorContext;

    // Viewport
    Viewport viewport;

    Level level;
#define HUD_TILE_COUNT 128

    // Sprite stufff
    bool flipCharacter = false;

#define METASPRITE_MAX_SPRITE_COUNT 64
#define MAX_METASPRITE_COUNT 256
    Rendering::Sprite metaspriteMemory[MAX_METASPRITE_COUNT * METASPRITE_MAX_SPRITE_COUNT];
    
    Editor::Metasprite::Metasprite characterMetasprite = {
        "FreyaIdle",
        30,
        metaspriteMemory
    };

    struct PlayerState {
        f32 x, y;
        f32 hSpeed, vSpeed;
    };

    PlayerState playerState{};
    f32 gravity = 562.5;

    u8 chrSheet[0x4000];

	void Initialize(Rendering::RenderContext* pRenderContext) {
        viewport.x = 0.0f;
        viewport.y = 96.0f;
        viewport.w = VIEWPORT_WIDTH_TILES * TILE_SIZE;
        viewport.h = VIEWPORT_HEIGHT_TILES * TILE_SIZE;

        // Init chr memory
        Rendering::Util::CreateChrSheet("chr000.bmp", chrSheet);
        Rendering::WriteChrMemory(pRenderContext, 0x4000, 0, chrSheet);
        Rendering::Util::CreateChrSheet("chr001.bmp", chrSheet);
        Rendering::WriteChrMemory(pRenderContext, 0x4000, 0x4000, chrSheet);
        Rendering::Util::CreateChrSheet("wings.bmp", chrSheet);

        playerState.x = 240;
        playerState.y = 128;

        // editorState.pLevel = &level;
        // editorState.pRenderContext = pContext;

        LoadLevel(&level, "test.lev");

        // Render all of first and second nametable
        for (int i = 0; i < NAMETABLE_COUNT; i++) {
            Rendering::WriteNametable(pRenderContext, i, NAMETABLE_SIZE - HUD_TILE_COUNT, HUD_TILE_COUNT, (u8*)&level.screens[i] + HUD_TILE_COUNT);
        }

        // SETTINGS
        pRenderSettings = Rendering::GetSettingsPtr(pRenderContext);

        // EDITOR
        pEditorContext = Editor::CreateEditorContext(pRenderContext);

        // Init metasprites
        Rendering::Sprite characterSprites[30] = {
            // Bow
            { -13, 2, 0x12, 0b00000001 }, { -13, 10, 0x13, 0b00000001 },
            { -5, 2, 0x14, 0b00000001 }, { -5, 10, 0x15, 0b00000001 },
            { -21, 10, 0x16, 0b00000001 }, { 3, 10, 0x17, 0b00000001 },
            // Freya
            { -16, -8, 0x08, 0b00000001 }, { -16, 0, 0x09, 0b00000001 },
            { -8, -8, 0x0A, 0b00000001 }, { -8, 0, 0x0B, 0b00000001 },
            { 0, -8, 0x0C, 0b00000001 }, { 0, 0, 0x0D, 0b00000001 },
            { 8, -8, 0x0E, 0b00000001 }, { 8, 0, 0x0F, 0b00000001 },
            // Left wing
            { -19, -18, 0x00, 0b00000001 }, { -19, -10, 0x01, 0b00000001 },
            { -11, -18, 0x02, 0b00000001 }, { -11, -10, 0x03, 0b00000001 },
            { -3, -18, 0x04, 0b00000001 }, { -3, -10, 0x05, 0b00000001 },
            { 5, -18, 0x06, 0b00000001 }, { 5, -10, 0x07, 0b00000001 },
            // Right wing
            { -19, 9, 0x00, 0b01000010 }, { -19, 1, 0x01, 0b01000010 },
            { -11, 9, 0x02, 0b01000010 }, { -11, 1, 0x03, 0b01000010 },
            { -3, 9, 0x04, 0b01000010 }, { -3, 1, 0x05, 0b01000010 },
            { 5, 9, 0x06, 0b01000010 }, { 5, 1, 0x07, 0b01000010 },
        };
        memcpy(metaspriteMemory, characterSprites, sizeof(Rendering::Sprite) * 30);
	}

    void Free() {
        Editor::FreeEditorContext(pEditorContext);
    }

    void UpdateHUD(Rendering::RenderContext* pContext, float dt) {
        float fps = 1.0f / dt;
        char hudText[128];
        snprintf(hudText, 64, " %4d FPS (%2d ms) (%04d, %04d) %s", (int)(fps), (int)(dt*1000), (int)viewport.x, (int)viewport.y, level.name);
        //snprintf(hudText + 64, 64, " Editor mode: %s, Palette: %#x ", Editor::GetEditorModeName(editorState.mode), editorState.palette);
        Rendering::WriteNametable(pContext, 0, 128, 0, (u8*)hudText);
    }

    void RenderHUD(Rendering::RenderContext* pContext) {
        Rendering::RenderState state = {
            0,
            0
        };
        Rendering::SetRenderState(pContext, 0, 16, state);
    }

    u32 DrawDebugCharacter(Rendering::RenderContext* pContext, u32 spriteOffset, s32 x, s32 y, bool flip, float time) {
        u32 msElapsed = time * 1000;
        s32 vOffset = (msElapsed / 360) % 2 ? -1 : 0;
        u32 wingFrame = (msElapsed / 180) % 4;
        Rendering::WriteChrMemory(pContext, 0x200, 0x4000, chrSheet + 0x200*wingFrame);
        Rendering::Util::WriteMetasprite(pContext, metaspriteMemory, characterMetasprite.spriteCount, spriteOffset, x, y + vOffset, flip);
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
        spriteOffset += DrawDebugCharacter(pRenderContext, spriteOffset, playerState.x - viewport.x, playerState.y - viewport.y, flipCharacter, secondsElapsed);

        // Editor::DrawSelection(&editorState, &viewport, spriteOffset);

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
            viewport.x,
            viewport.y
        };
        Rendering::SetRenderState(pRenderContext, 16, 272, state);

        // GUI
        Rendering::BeginImGuiFrame(pRenderContext);
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        Editor::Debug::DrawDebugWindow(pEditorContext, pRenderContext);
        Editor::Metasprite::DrawPreviewWindow(pEditorContext, &characterMetasprite);
        Editor::Metasprite::DrawSpriteListWindow(pEditorContext, &characterMetasprite);
        Editor::Metasprite::DrawSpriteEditor(pEditorContext, &characterMetasprite);
        Editor::CHR::DrawCHRWindow(pEditorContext);
        Editor::Collision::DrawCollisionEditor(pEditorContext, pRenderContext);
        ImGui::Render();
        Rendering::Render(pRenderContext);
    }

    void Step(float dt, Rendering::RenderContext* pContext) {
        secondsElapsed += dt;

        // Poll input
        controllerState = Input::PollInput(controllerState);

        // Editor::HandleInput(&editorState, controllerState, controllerStatePrev, &viewport, dt);

        if (controllerState & Input::ControllerState::Left) {
            flipCharacter = true;
            //characterSprites = &characterFwd;
            Rendering::WriteChrMemory(pContext, 0x200, 0x4200, chrSheet + 0xA00);
            //xBowOffset = 19;
            playerState.hSpeed = -100;
        }
        else if (controllerState & Input::ControllerState::Right) {
            flipCharacter = false;
            //characterSprites = &characterFwd;
            Rendering::WriteChrMemory(pContext, 0x200, 0x4200, chrSheet + 0xA00);
            //xBowOffset = 19;
            playerState.hSpeed = 100;
        }
        else {
            //characterSprites = &characterIdle;
            Rendering::WriteChrMemory(pContext, 0x200, 0x4200, chrSheet + 0x800);
            //xBowOffset = 18;
            playerState.hSpeed = 0;
        }

        if ((controllerState & Input::ControllerState::Start) && !(controllerStatePrev & Input::ControllerState::Start)) {
            pRenderSettings->useCRTFilter = !pRenderSettings->useCRTFilter;
        }

        if ((controllerState & Input::ControllerState::A) && !(controllerStatePrev & Input::ControllerState::A)) {
            playerState.vSpeed = -250;
        }

        if (playerState.vSpeed < 0 && !(controllerState & Input::ControllerState::A) && (controllerStatePrev & Input::ControllerState::A)) {
            playerState.vSpeed /= 2;
        }

        if (!(controllerState & Input::ControllerState::A) || playerState.vSpeed < 0) {
            playerState.vSpeed += gravity * dt;
        } else playerState.vSpeed += (gravity / 4) * dt;

        u32 xPlayerCollision = playerState.x;
        u32 yPlayerCollision = playerState.y + 16;

        Collision::TileCollision* bgCollision = Collision::GetBgCollisionPtr();
        if (playerState.vSpeed > 0 && yPlayerCollision <= NAMETABLE_HEIGHT_TILES * TILE_SIZE) {
            u32 screenIndex = xPlayerCollision / (NAMETABLE_WIDTH_TILES * TILE_SIZE);
            f32 screenRelativeX = xPlayerCollision - screenIndex * NAMETABLE_WIDTH_TILES * TILE_SIZE;
            u32 nametableIndex = screenIndex % NAMETABLE_COUNT;
            u32 yTile = yPlayerCollision / TILE_SIZE;
            u32 xTile = screenRelativeX / TILE_SIZE;
            u32 tileIndex = yTile * NAMETABLE_WIDTH_TILES + xTile;
            u8 collidingTile;
            Rendering::ReadNametable(pContext, nametableIndex, 1, tileIndex, &collidingTile);
            if (bgCollision[collidingTile].type == Collision::TileSolid) {
                playerState.vSpeed = 0;
                playerState.y -= yPlayerCollision % TILE_SIZE;
            }
        }
        playerState.y += playerState.vSpeed * dt;
        playerState.x += playerState.hSpeed * dt;

        controllerStatePrev = controllerState;

        Render(pContext, dt);

        // Corrupt CHR mem
        //int randomInt = rand();
        //Rendering::WriteChrMemory(pContext, sizeof(int), rand() % (CHR_MEMORY_SIZE - sizeof(int)), (u8*)&randomInt);

        // Animate palette
        static float paletteAccumulator = 0;
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
        }
    }
}