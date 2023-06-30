#include "game.h"
#include "system.h"
#include "input.h"
#include <math.h>
#include <string.h>
#include "rendering_util.h"
#include "level.h"
#include "viewport.h"
#include "editor.h"
#include <imgui.h>

namespace Game {
    r32 secondsElapsed = 0.0f;

    // Settings
    Rendering::Settings* pRenderSettings;

    // Input
    Input::ControllerState controllerState;
    Input::ControllerState controllerStatePrev;

    // Editor
    Editor::EditorState editorState;

    // Viewport
    Viewport viewport;

    Level level;
#define HUD_TILE_COUNT 128

    // Sprite stufff
    bool flipCharacter = false;

    Rendering::Sprite characterMetasprite[30] = {
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

    Rendering::Sprite test[4] = {
        { -8, -8, 0x08, 0b00000001 }, { -8, 0, 0x08, 0b00000001 },
        { 0, -8, 0x08, 0b00000001 }, { 0, 0, 0x08, 0b00000001 },
    };

    enum TileType : u8 {
        TileEmpty = 0,
        TileSolid = 1 << 0,
        TilePassThrough = 1 << 1,
        TileJumpThrough = 1 << 2,
        TilePassThroughFlip = 1 << 3
    };

    TileType bgCollision[256]{
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileSolid, TileSolid, TileSolid, TileSolid, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileSolid, TileSolid, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileSolid, TileSolid, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileSolid, TileSolid, TileSolid, TileSolid, TilePassThrough, TilePassThrough, TilePassThrough, TilePassThrough, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
        TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty, TileEmpty,
    };

    struct PlayerState {
        f32 x, y;
        f32 hSpeed, vSpeed;
    };

    PlayerState playerState{};
    f32 gravity = 562.5;

    // DEBUG
    u8 paletteIndex = 0;
    ImTextureID* debugChrTexture;
    ImTextureID debugPaletteTexture;

    u8 chrSheet[0x4000];

	void Initialize(Rendering::RenderContext* pContext) {
        viewport.x = 0.0f;
        viewport.y = 96.0f;
        viewport.w = VIEWPORT_WIDTH_TILES * TILE_SIZE;
        viewport.h = VIEWPORT_HEIGHT_TILES * TILE_SIZE;

        // Init chr memory
        Rendering::Util::CreateChrSheet("chr000.bmp", chrSheet);
        Rendering::WriteChrMemory(pContext, 0x4000, 0, chrSheet);
        Rendering::Util::CreateChrSheet("chr001.bmp", chrSheet);
        Rendering::WriteChrMemory(pContext, 0x4000, 0x4000, chrSheet);
        Rendering::Util::CreateChrSheet("wings.bmp", chrSheet);

        playerState.x = 240;
        playerState.y = 128;

        editorState.pLevel = &level;
        editorState.pRenderContext = pContext;

        LoadLevel(&level, "test.lev");

        // Render all of first and second nametable
        for (int i = 0; i < NAMETABLE_COUNT; i++) {
            Rendering::WriteNametable(pContext, i, NAMETABLE_SIZE - HUD_TILE_COUNT, HUD_TILE_COUNT, (u8*)&level.screens[i] + HUD_TILE_COUNT);
        }

        // DEBUG
        pRenderSettings = Rendering::GetSettingsPtr(pContext);
        debugChrTexture = Rendering::SetupDebugChrRendering(pContext);
        debugPaletteTexture = Rendering::SetupDebugPaletteRendering(pContext);
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

        Rendering::Sprite outSprites[30]{};
        for (int i = 0; i < 30; i++) {
            Rendering::Sprite sprite = characterMetasprite[i];
            if (flip) {
                sprite.attributes = sprite.attributes ^ 0b01000000;
                sprite.x *= -1;
            }
            sprite.y += y + vOffset;
            sprite.x += x;
            outSprites[i] = sprite;
        }

        Rendering::WriteSprites(pContext, 30, spriteOffset, outSprites);
        return 30;
    }

    ImVec2 DrawTileGrid(r32 size, s32 divisions) {
        r32 gridStep = size / divisions;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 topLeft = ImGui::GetCursorScreenPos();
        ImVec2 btmRight = ImVec2(topLeft.x + size, topLeft.y + size);
        ImGui::Image(debugPaletteTexture, ImVec2(size, size), ImVec2(0, 0), ImVec2(0.015625f, 1.0f));
        // drawList->AddImage(debugPaletteTexture, topLeft, btmRight, ImVec2(0, 0), ImVec2(0.015625f, 1.0f));
        //drawList->AddRectFilled(topLeft, btmRight, IM_COL32(50, 50, 50, 255));
        //drawList->AddRect(topLeft, btmRight, IM_COL32(255, 255, 255, 255));
        for (r32 x = 0; x < size; x += gridStep)
            drawList->AddLine(ImVec2(topLeft.x + x, topLeft.y), ImVec2(topLeft.x + x, btmRight.y), IM_COL32(200, 200, 200, 40));
        for (r32 y = 0; y < size; y += gridStep)
            drawList->AddLine(ImVec2(topLeft.x, topLeft.y + y), ImVec2(btmRight.x, topLeft.y + y), IM_COL32(200, 200, 200, 40));

        return topLeft;
    }

    ImVec2 GetTileCoord(u8 index) {
        ImVec2 coord = ImVec2(index % 16, index / 16);
        return coord;
    }

    ImVec2 TileCoordToTexCoord(ImVec2 coord, bool chrIndex) {
        ImVec2 normalizedCoord = ImVec2(coord.x / 32.0f, coord.y / 16.0f);
        if (chrIndex) {
            normalizedCoord.x += 0.5;
        }
        return normalizedCoord;
    }

    void DrawNametable(ImVec2 tablePos, u8 *pNametable) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (int i = 0; i < NAMETABLE_SIZE; i++) {
            s32 x = i % NAMETABLE_WIDTH_TILES;
            s32 y = i / NAMETABLE_WIDTH_TILES;

            u8 tile = pNametable[i];
            ImVec2 tileCoord = GetTileCoord(tile);
            ImVec2 tileStart = TileCoordToTexCoord(tileCoord, 0);
            ImVec2 tileEnd = TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 0);
            ImVec2 pos = ImVec2(tablePos.x + x * 8, tablePos.y + y * 8);

            // Palette from attribs
            s32 xBlock = x / 4;
            s32 yBlock = y / 4;
            s32 smallBlockOffset = (x % 4 / 2) + (y % 4 / 2) * 2;
            s32 blockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
            s32 nametableOffset = NAMETABLE_ATTRIBUTE_OFFSET + blockIndex;
            u8 attribute = pNametable[nametableOffset];
            u8 paletteIndex = (attribute >> (smallBlockOffset * 2)) & 0b11;

            drawList->AddImage(debugChrTexture[paletteIndex], pos, ImVec2(pos.x + 8, pos.y + 8), tileStart, tileEnd);
        }
    }

    void DrawDebugWindow(Rendering::RenderContext* pContext) {
        ImGui::Begin("Debug");

        ImGui::Checkbox("CRT filter", (bool*)&(pRenderSettings->useCRTFilter));

        if (ImGui::TreeNode("Sprites")) {
            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
            if (ImGui::BeginTable("sprites", 4, flags)) {
                ImGui::TableSetupColumn("Sprite");
                ImGui::TableSetupColumn("Pos");
                ImGui::TableSetupColumn("Tile");
                ImGui::TableSetupColumn("Attr");
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();

                static Rendering::Sprite sprites[MAX_SPRITE_COUNT];
                Rendering::ReadSprites(pContext, MAX_SPRITE_COUNT, 0, sprites);
                for (int i = 0; i < MAX_SPRITE_COUNT; i++) {
                    Rendering::Sprite sprite = sprites[i];
                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%04d", i);
                    ImGui::TableNextColumn();
                    ImGui::Text("(%d, %d)", sprite.x, sprite.y);
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%02x", sprite.tileId);
                    ImGui::TableNextColumn();
                    ImGui::Text("0x%02x", sprite.attributes);
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("CHR")) {
            for (int i = 0; i < 8; i++) {
                ImGui::PushID(i);
                if (ImGui::ImageButton("", debugPaletteTexture, ImVec2(80, 10), ImVec2(0.125 * i, 0), ImVec2(0.125 * (i + 1), 1)))
                    paletteIndex = i;
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::NewLine();

            const u32 chrSize = 384;

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImVec2 chrPos = DrawTileGrid(chrSize, 16);
            //ImGui::Image(debugChrTexture, ImVec2(chrSize, chrSize), ImVec2(0, 0), ImVec2(0.5, 1));
            drawList->AddImage(debugChrTexture[paletteIndex], chrPos, ImVec2(chrPos.x + chrSize, chrPos.y + chrSize), ImVec2(0, 0), ImVec2(0.5, 1));
            ImGui::SameLine();
            chrPos = DrawTileGrid(chrSize, 16);
            //ImGui::Image(debugChrTexture, ImVec2(chrSize, chrSize), ImVec2(0.5, 0), ImVec2(1, 1));
            drawList->AddImage(debugChrTexture[paletteIndex], chrPos, ImVec2(chrPos.x + chrSize, chrPos.y + chrSize), ImVec2(0.5, 0), ImVec2(1, 1));
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Nametables")) {
            ImVec2 tablePos = DrawTileGrid(512, 64);
            static u8 nametable[NAMETABLE_SIZE];
            Rendering::ReadNametable(pContext, 0, NAMETABLE_SIZE, 0, nametable);
            DrawNametable(tablePos, nametable);
            ImGui::SameLine();
            tablePos = DrawTileGrid(512, 64);
            Rendering::ReadNametable(pContext, 1, NAMETABLE_SIZE, 0, nametable);
            DrawNametable(tablePos, nametable);
            ImGui::TreePop();
        }

        ImGui::End();
    }

    void DrawMetaspritePreviewWindow(Rendering::RenderContext* pContext) {
        ImGui::Begin("Metasprite preview");

        s32 scale = 3;
        s32 gridSize = 64 * scale;
        s32 gridStep = TILE_SIZE * scale;
        ImVec2 gridPos = DrawTileGrid(gridSize, 8);
        ImVec2 origin = ImVec2(gridPos.x + gridSize / 2, gridPos.y + gridSize / 2);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(origin.x - 10, origin.y), ImVec2(origin.x + 10, origin.y), IM_COL32(200, 200, 200, 255));
        drawList->AddLine(ImVec2(origin.x, origin.y - 10), ImVec2(origin.x, origin.y + 10), IM_COL32(200, 200, 200, 255));

        for (int i = 29; i >= 0; i--) {
            Rendering::Sprite sprite = characterMetasprite[i];
            u8 index = (u8)sprite.tileId;
            ImVec2 tileCoord = GetTileCoord(index);
            ImVec2 tileStart = TileCoordToTexCoord(tileCoord, 1);
            ImVec2 tileEnd = TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 1);
            ImVec2 pos = ImVec2(origin.x + scale * sprite.x, origin.y + scale * sprite.y);
            bool flipX = sprite.attributes & 0b01000000;
            bool flipY = sprite.attributes & 0b10000000;
            u8 palette = (sprite.attributes & 3) + 4;
            drawList->AddImage(debugChrTexture[palette], pos, ImVec2(pos.x + gridStep, pos.y + gridStep), ImVec2(flipX ? tileEnd.x : tileStart.x, flipY ? tileEnd.y : tileStart.y), ImVec2(!flipX ? tileEnd.x : tileStart.x, !flipY ? tileEnd.y : tileStart.y));
        }
        ImGui::End();
    }

    void Render(Rendering::RenderContext* pContext, float dt) {
        Rendering::SetCurrentTime(pContext, secondsElapsed);

        // Rendering::ClearSprites(pContext, 256);

        u32 spriteOffset = 0;
        /*for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                spriteOffset += DrawDebugCharacter(pContext, spriteOffset, xCharacter + x * 32, yCharacter + y * 32, secondsElapsed + x*0.1 + y*0.8);
            }
        }*/
        spriteOffset += DrawDebugCharacter(pContext, spriteOffset, playerState.x - viewport.x, playerState.y - viewport.y, flipCharacter, secondsElapsed);

        // Editor::DrawSelection(&editorState, &viewport, spriteOffset);

        UpdateHUD(pContext, dt);
        RenderHUD(pContext);

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
        Rendering::SetRenderState(pContext, 16, 272, state);

        // GUI
        Rendering::BeginImGuiFrame(pContext);
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        DrawDebugWindow(pContext);
        DrawMetaspritePreviewWindow(pContext);
        ImGui::Render();
        Rendering::Render(pContext);
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

        if (playerState.vSpeed > 0 && yPlayerCollision <= NAMETABLE_HEIGHT_TILES * TILE_SIZE) {
            u32 screenIndex = xPlayerCollision / (NAMETABLE_WIDTH_TILES * TILE_SIZE);
            f32 screenRelativeX = xPlayerCollision - screenIndex * NAMETABLE_WIDTH_TILES * TILE_SIZE;
            u32 nametableIndex = screenIndex % NAMETABLE_COUNT;
            u32 yTile = yPlayerCollision / TILE_SIZE;
            u32 xTile = screenRelativeX / TILE_SIZE;
            u32 tileIndex = yTile * NAMETABLE_WIDTH_TILES + xTile;
            u8 collidingTile;
            Rendering::ReadNametable(pContext, nametableIndex, 1, tileIndex, &collidingTile);
            if (bgCollision[collidingTile] == TileSolid) {
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