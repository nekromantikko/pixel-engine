#include "editor_level.h"
#include "editor_util.h"
#include "rendering_util.h"
#include "game.h"
#include "viewport.h"
#include "level.h"
#include "tileset.h"
#include "math.h"
#include <cstdio>
#include "system.h"

namespace Editor {
	namespace LevelEditor {
        constexpr u32 levelTypeCount = 3;
        constexpr const char* levelTypeNames[levelTypeCount] = { "Sidescroller", "World map", "Title screen" };

        constexpr u32 actorTypeCount = 3;
        constexpr const char* actorTypeNames[actorTypeCount] = { "None", "Door", "Skull" };

        Level::ActorType selectedActorType = Level::ACTOR_NONE;
        bool actorMode = false;

        u32 selectedLevel = 0;

        static void DrawActor(const ImVec2 contentTopLeft, const ImVec2 contentBtmRight, const ImVec2 actorTopLeft, const r32 metatileDrawSize, const Level::ActorType type, const u8 opacity) {
            if (type == Level::ACTOR_NONE) {
                return;
            }

            const char* label = actorTypeNames[type];
            const ImVec2 textSize = ImGui::CalcTextSize(label);
            const ImVec2 textPos = ImVec2(actorTopLeft.x + metatileDrawSize / 2 - textSize.x / 2, actorTopLeft.y + metatileDrawSize / 2 - textSize.y / 2);

            // Manual clipping
            const ImVec2 clippedActorTopLeft = ImVec2(
                Max(actorTopLeft.x, contentTopLeft.x),
                Max(actorTopLeft.y, contentTopLeft.y)
            );
            const ImVec2 clippedActorBtmRight = ImVec2(
                Min(actorTopLeft.x + metatileDrawSize, contentBtmRight.x),
                Min(actorTopLeft.y + metatileDrawSize, contentBtmRight.y)
            );

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(clippedActorTopLeft, clippedActorBtmRight, IM_COL32(0, 255, 255, opacity));
            drawList->PushClipRect(clippedActorTopLeft, clippedActorBtmRight);
            drawList->AddText(textPos, IM_COL32(255, 255, 255, opacity), label);
            drawList->PopClipRect();
        }

        static void DrawActors(const Level::Level* pLevel, const Viewport* pViewport, const ImVec2 topLeft, const ImVec2 btmRight, const r32 tileDrawSize) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const r32 metatileDrawSize = tileDrawSize * Tileset::metatileWorldSize;

            const s32 screenStartX = pViewport->x / Level::screenWidthTiles;
            const s32 screenStartY = pViewport->y / Level::screenHeightTiles;

            const s32 screenEndX = (pViewport->x + VIEWPORT_WIDTH_TILES) / Level::screenWidthTiles;
            const s32 screenEndY = (pViewport->y + VIEWPORT_HEIGHT_TILES) / Level::screenHeightTiles;

            for (s32 y = screenStartY; y <= screenEndY; y++) {
                for (s32 x = screenStartX; x <= screenEndX; x++) {
                    const Vec2 screenPos = { x * Level::screenWidthTiles, y * Level::screenHeightTiles };
                    const Vec2 screenViewportCoords = { (screenPos.x - pViewport->x) * tileDrawSize, (screenPos.y - pViewport->y) * tileDrawSize };

                    const s32 i = x + y * pLevel->width;
                    const Level::Screen& screen = pLevel->screens[i];

                    static char screenLabelText[16];
                    snprintf(screenLabelText, 16, "%#04x", i);

                    const ImVec2 lineStart = ImVec2(topLeft.x + screenViewportCoords.x, topLeft.y + screenViewportCoords.y);
                    drawList->AddLine(lineStart, ImVec2(btmRight.x, topLeft.y + screenViewportCoords.y), IM_COL32(255, 255, 255, 255), 1.0f);
                    drawList->AddLine(lineStart, ImVec2(topLeft.x + screenViewportCoords.x, btmRight.y), IM_COL32(255, 255, 255, 255), 1.0f);

                    const ImVec2 textPos = ImVec2(topLeft.x + screenViewportCoords.x + tileDrawSize, topLeft.y + screenViewportCoords.y + tileDrawSize);
                    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), screenLabelText);

                    for (u32 y = 0; y < Level::screenHeightMetatiles; y++) {
                        for (u32 x = 0; x < Level::screenWidthMetatiles; x++) {
                            ImVec2 actorTopLeft = ImVec2(topLeft.x + screenViewportCoords.x + x * metatileDrawSize, topLeft.y + screenViewportCoords.y + y * metatileDrawSize);
                            ImVec2 actorBtmRight = ImVec2(actorTopLeft.x + metatileDrawSize, actorTopLeft.y + metatileDrawSize);

                            if (actorBtmRight.x < topLeft.x || actorBtmRight.y < topLeft.y || actorTopLeft.x >= btmRight.x || actorTopLeft.y >= btmRight.y) {
                                continue;
                            }

                            const u32 tileIndex = x + Level::screenWidthMetatiles * y;
                            const Level::ActorType type = screen.tiles[tileIndex].actorType;

                            DrawActor(topLeft, btmRight, actorTopLeft, metatileDrawSize, type, actorMode ? 127 : 15);
                        }
                    }
                }
            }
        }

		void DrawGameWindow(EditorContext* pEditorContext, Rendering::RenderContext* pRenderContext) {
            ImGui::Begin("Game");

            Level::Level* pLevel = Game::GetLevel();
            const bool noLevelLoaded = pLevel == nullptr;

            bool editMode = Game::IsPaused() && !noLevelLoaded;

            ImGui::BeginDisabled(noLevelLoaded);
            if (ImGui::Button(editMode ? "Play mode" : "Edit mode")) {
                Game::ReloadLevel();
                Game::SetPaused(!editMode);
                editMode = !editMode;
            }
            ImGui::EndDisabled();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 topLeft = ImGui::GetCursorScreenPos();
            static const r32 aspectRatio = (r32)VIEWPORT_WIDTH_TILES / VIEWPORT_HEIGHT_TILES;
            const r32 contentWidth = ImGui::GetContentRegionAvail().x;
            const r32 contentHeight = contentWidth / aspectRatio;
            const r32 renderScale = contentWidth / (VIEWPORT_WIDTH_TILES * TILE_SIZE);
            ImVec2 btmRight = ImVec2(topLeft.x + contentWidth, topLeft.y + contentHeight);
            drawList->AddImage(pEditorContext->gameViewTexture, topLeft, btmRight);

            if (editMode) {
                Viewport* pViewport = Game::GetViewport();
                Rendering::Nametable* pNametables = Rendering::GetNametablePtr(pRenderContext, 0);

                // Toggle actor mode
                ImGui::SameLine();
                if (ImGui::Button(actorMode ? "Tile mode" : "Actor mode")) {
                    actorMode = !actorMode;
                }

                // Save and reload
                ImGui::SameLine();
                if (ImGui::Button("Save level")) {
                    Level::SaveLevels("assets/levels.lev");
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert changes")) {
                    Level::LoadLevels("assets/levels.lev");
                    RefreshViewport(pViewport, pNametables, pLevel);
                }
                ImGui::SameLine();
                if (ImGui::Button("Refresh viewport")) {
                    RefreshViewport(pViewport, pNametables, pLevel);
                }

                // Invisible button to prevent dragging window
                ImGui::InvisibleButton("##canvas", ImVec2(contentWidth, contentHeight));

                drawList->PushClipRect(topLeft, btmRight);

                ImGuiIO& io = ImGui::GetIO();

                // View scrolling
                static ImVec2 dragStartPos = ImVec2(0, 0);
                static ImVec2 dragDelta = ImVec2(0, 0);
                bool scrolling = false;

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    dragStartPos = io.MousePos;
                }

                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    const ImVec2 newDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
                    const r32 dx = -(newDelta.x - dragDelta.x) / renderScale / TILE_SIZE;
                    const r32 dy = -(newDelta.y - dragDelta.y) / renderScale / TILE_SIZE;
                    dragDelta = newDelta;

                    MoveViewport(pViewport, pNametables, pLevel, dx, dy);
                    scrolling = true;
                }

                // Reset drag delta when mouse released
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    dragDelta = ImVec2(0, 0);
                }

                const r32 tileDrawSize = TILE_SIZE * renderScale;
                DrawActors(pLevel, pViewport, topLeft, btmRight, tileDrawSize);

                // If mouse over
                if (io.MousePos.x >= topLeft.x && io.MousePos.x < btmRight.x && io.MousePos.y >= topLeft.y && io.MousePos.y < btmRight.y) {
                    const ImVec2 mousePosInViewportCoords = ImVec2((io.MousePos.x - topLeft.x) / tileDrawSize, (io.MousePos.y - topLeft.y) / tileDrawSize);
                    const ImVec2 mousePosInWorldCoords = ImVec2(mousePosInViewportCoords.x + pViewport->x, mousePosInViewportCoords.y + pViewport->y);
                    const ImVec2 hoveredMetatileWorldPos = ImVec2(floorf(mousePosInWorldCoords.x / Tileset::metatileWorldSize) * Tileset::metatileWorldSize, floorf(mousePosInWorldCoords.y / Tileset::metatileWorldSize) * Tileset::metatileWorldSize);
                    static bool selecting = false;

                    // Selection
                    if (!scrolling && !actorMode) {
                        static ImVec2 selectionStartPos = ImVec2(0, 0);
                        static ImVec2 selectionTopLeft;
                        static ImVec2 selectionBtmRight;

                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                            selectionStartPos = hoveredMetatileWorldPos;
                            selectionTopLeft = hoveredMetatileWorldPos;
                            selectionBtmRight = ImVec2(hoveredMetatileWorldPos.x + Tileset::metatileWorldSize, hoveredMetatileWorldPos.y + Tileset::metatileWorldSize);
                            selecting = true;
                        }

                        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                            selectionTopLeft = ImVec2(Min(selectionStartPos.x, hoveredMetatileWorldPos.x), Min(selectionStartPos.y, hoveredMetatileWorldPos.y));
                            selectionBtmRight = ImVec2(Max(selectionStartPos.x, hoveredMetatileWorldPos.x) + Tileset::metatileWorldSize, Max(selectionStartPos.y, hoveredMetatileWorldPos.y) + Tileset::metatileWorldSize);

                            const ImVec2 selectionTopLeftInPixelCoords = ImVec2((selectionTopLeft.x - pViewport->x) * tileDrawSize + topLeft.x, (selectionTopLeft.y - pViewport->y) * tileDrawSize + topLeft.y);
                            const ImVec2 selectionBtmRightInPixelCoords = ImVec2((selectionBtmRight.x - pViewport->x) * tileDrawSize + topLeft.x, (selectionBtmRight.y - pViewport->y) * tileDrawSize + topLeft.y);

                            drawList->AddRectFilled(selectionTopLeftInPixelCoords, selectionBtmRightInPixelCoords, IM_COL32(255, 255, 255, 63));
                        }

                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
                            selecting = false;
                            u32 selectionWidth = (selectionBtmRight.x - selectionTopLeft.x) / Tileset::metatileWorldSize;
                            u32 selectionHeight = (selectionBtmRight.y - selectionTopLeft.y) / Tileset::metatileWorldSize;

                            pEditorContext->levelSelectionSize = ImVec2((r32)selectionWidth, (r32)selectionHeight);
                            pEditorContext->levelSelectionOffset = ImVec2(selectionTopLeft.x - hoveredMetatileWorldPos.x, selectionTopLeft.y - hoveredMetatileWorldPos.y);

                            for (u32 x = 0; x < selectionWidth; x++) {
                                for (u32 y = 0; y < selectionHeight; y++) {
                                    u32 clipboardIndex = y * selectionWidth + x;

                                    const Vec2 metatileWorldPos = { selectionTopLeft.x + x * Tileset::metatileWorldSize, selectionTopLeft.y + y * Tileset::metatileWorldSize };

                                    const u32 screenIndex = WorldToScreenIndex(pLevel, metatileWorldPos);
                                    const u32 screenTileIndex = Level::WorldToMetatileIndex(metatileWorldPos);
                                    const u8 metatileIndex = pLevel->screens[screenIndex].tiles[screenTileIndex].metatile;

                                    pEditorContext->levelClipboard[clipboardIndex] = metatileIndex;
                                }
                            }
                        }
                    }

                    if (!selecting) {
                        if (actorMode) {
                            const ImVec2 metatileInViewportCoords = ImVec2(hoveredMetatileWorldPos.x - pViewport->x, hoveredMetatileWorldPos.y - pViewport->y);
                            const ImVec2 metatileInPixelCoords = ImVec2(metatileInViewportCoords.x * tileDrawSize + topLeft.x, metatileInViewportCoords.y * tileDrawSize + topLeft.y);
                            const r32 metatileDrawSize = tileDrawSize * Tileset::metatileWorldSize;

                            const u32 screenIndex = WorldToScreenIndex(pLevel, { hoveredMetatileWorldPos.x, hoveredMetatileWorldPos.y });

                            DrawActor(topLeft, btmRight, metatileInPixelCoords, metatileDrawSize, selectedActorType, 63);
                            drawList->AddRect(metatileInPixelCoords, ImVec2(metatileInPixelCoords.x + metatileDrawSize, metatileInPixelCoords.y + metatileDrawSize), IM_COL32(255, 255, 255, 255));

                            // Paint actors
                            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                if (screenIndex < pLevel->width * pLevel->height) {
                                    const u32 screenTileIndex = Level::WorldToMetatileIndex({ hoveredMetatileWorldPos.x, hoveredMetatileWorldPos.y });

                                    pLevel->screens[screenIndex].tiles[screenTileIndex].actorType = selectedActorType;
                                }
                            }
                        }
                        else {
                            const u32 clipboardWidth = (u32)pEditorContext->levelSelectionSize.x;
                            const u32 clipboardHeight = (u32)pEditorContext->levelSelectionSize.y;
                            const ImVec2 clipboardTopLeft = ImVec2(hoveredMetatileWorldPos.x + pEditorContext->levelSelectionOffset.x, hoveredMetatileWorldPos.y + pEditorContext->levelSelectionOffset.y);
                            const ImVec2 clipboardBtmRight = ImVec2(clipboardTopLeft.x + clipboardWidth * Tileset::metatileWorldSize, clipboardTopLeft.y + clipboardHeight * Tileset::metatileWorldSize);
                            for (u32 x = 0; x < clipboardWidth; x++) {
                                for (u32 y = 0; y < clipboardHeight; y++) {
                                    u32 clipboardIndex = y * clipboardWidth + x;
                                    const Vec2 metatileWorldPos = { clipboardTopLeft.x + x * Tileset::metatileWorldSize, clipboardTopLeft.y + y * Tileset::metatileWorldSize };
                                    const ImVec2 metatileInViewportCoords = ImVec2(metatileWorldPos.x - pViewport->x, metatileWorldPos.y - pViewport->y);
                                    const ImVec2 metatileInPixelCoords = ImVec2(metatileInViewportCoords.x * tileDrawSize + topLeft.x, metatileInViewportCoords.y * tileDrawSize + topLeft.y);
                                    const u8 metatileIndex = pEditorContext->levelClipboard[clipboardIndex];

                                    Util::DrawMetatile(pEditorContext, metatileIndex, metatileInPixelCoords, tileDrawSize, IM_COL32(255, 255, 255, 127));

                                    // Paint metatiles
                                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                        u32 screenIndex = WorldToScreenIndex(pLevel, metatileWorldPos);
                                        if (screenIndex >= pLevel->width * pLevel->height) {
                                            continue;
                                        }

                                        const Vec2 screenPos = Level::WorldToScreenOffset(metatileWorldPos);
                                        const u32 screenTileIndex = ScreenOffsetToMetatileIndex(pLevel, screenPos);

                                        pLevel->screens[screenIndex].tiles[screenTileIndex].metatile = metatileIndex;
                                        const u32 nametableIndex = Level::WorldToNametableIndex(metatileWorldPos);

                                        Tileset::CopyMetatileToNametable(&pNametables[nametableIndex], (u16)metatileWorldPos.x % NAMETABLE_WIDTH_TILES, (u16)metatileWorldPos.y % NAMETABLE_HEIGHT_TILES, metatileIndex);
                                    }
                                }
                            }

                            const ImVec2 clipboardTopLeftInPixelCoords = ImVec2((clipboardTopLeft.x - pViewport->x) * tileDrawSize + topLeft.x, (clipboardTopLeft.y - pViewport->y) * tileDrawSize + topLeft.y);
                            const ImVec2 clipboardBtmRightInPixelCoords = ImVec2((clipboardBtmRight.x - pViewport->x) * tileDrawSize + topLeft.x, (clipboardBtmRight.y - pViewport->y) * tileDrawSize + topLeft.y);
                            drawList->AddRect(clipboardTopLeftInPixelCoords, clipboardBtmRightInPixelCoords, IM_COL32(255, 255, 255, 255));
                        }
                    }
                }
                drawList->PopClipRect();
            }

            ImGui::End();
		}

        void DrawActorList() {
            ImGui::Begin("Actors");

            for (u32 i = 0; i < actorTypeCount; i++) {
                const char* actorTypeName = actorTypeNames[i];
                if (ImGui::Selectable(actorTypeName, selectedActorType == i)) {
                    selectedActorType = (Level::ActorType)i;
                }
            }

            ImGui::End();
        }

        void DrawLevelList(Rendering::RenderContext* pRenderContext) {
            ImGui::Begin("Levels");

            Level::Level* pCurrentLevel = Game::GetLevel();
            Viewport* pViewport = Game::GetViewport();
            Rendering::Nametable* pNametables = Rendering::GetNametablePtr(pRenderContext, 0);

            Level::Level* pLevels = Level::GetLevelsPtr();
            Level::Level& level = pLevels[selectedLevel];

            const bool editingCurrentLevel = pCurrentLevel == &level;
            const bool editMode = Game::IsPaused();

            if (ImGui::Button("Save")) {
                Level::SaveLevels("assets/levels.lev");
            }
            ImGui::SameLine();
            if (ImGui::Button("Revert changes")) {
                Level::LoadLevels("assets/levels.lev");

                if (editingCurrentLevel) {
                    RefreshViewport(pViewport, pNametables, pCurrentLevel);
                }
            }

            static constexpr u32 maxLabelNameLength = Level::levelMaxNameLength + 8;
            char label[maxLabelNameLength];
            snprintf(label, maxLabelNameLength, "%#04x: %s", selectedLevel, pLevels[selectedLevel].name);

            if (ImGui::BeginCombo("Level", label))
            {
                for (u32 i = 0; i < Level::maxLevelCount; i++)
                {
                    ImGui::PushID(i);

                    snprintf(label, maxLabelNameLength, "%#04x: %s", i, pLevels[i].name);

                    const bool selected = selectedLevel == i;
                    if (ImGui::Selectable(label, selected)) {
                        selectedLevel = i;
                    }

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                Game::LoadLevel(selectedLevel);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(selectedLevel == 0);
            if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
                Level::SwapLevels(selectedLevel, selectedLevel - 1);
                selectedLevel -= 1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(selectedLevel == Level::maxLevelCount - 1);
            if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
                Level::SwapLevels(selectedLevel, selectedLevel + 1);
                selectedLevel += 1;
            }
            ImGui::EndDisabled();

            //ImGui::BeginDisabled(editingCurrentLevel && !editMode);

            ImGui::InputText("Name", level.name, Level::levelMaxNameLength);

            if (ImGui::BeginCombo("Type", levelTypeNames[(int)level.type])) {
                for (u32 i = 0; i < levelTypeCount; i++) {
                    ImGui::PushID(i);

                    const bool selected = level.type == i;
                    if (ImGui::Selectable(levelTypeNames[i], selected)) {
                        level.type = (Level::LevelType)i;
                    }

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }

            s32 size[2] = { level.width, level.height };
            if (ImGui::InputInt2("Size", size)) {
                if (size[0] >= 1 && size[1] >= 1 && size[0] * size[1] <= Level::levelMaxScreenCount) {
                    level.width = size[0];
                    level.height = size[1];
                }
            }

            // Screens
            if (ImGui::TreeNode("Screens")) {
                // TODO: Lay these out nicer
                u32 screenCount = level.width * level.height;
                for (u32 i = 0; i < screenCount; i++) {
                    Level::Screen& screen = level.screens[i];

                    if (ImGui::TreeNode(&screen, "%#04x", i)) {

                        const Level::Level& exitTargetLevel = pLevels[screen.exitTargetLevel];

                        if (ImGui::BeginCombo("Exit target level", exitTargetLevel.name)) {
                            for (u32 i = 0; i < Level::maxLevelCount; i++)
                            {
                                ImGui::PushID(i);
                                const bool selected = screen.exitTargetLevel == i;
                                if (ImGui::Selectable(pLevels[i].name, selected)) {
                                    screen.exitTargetLevel = i;
                                }

                                if (selected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndCombo();
                        }

                        int exitScreen = screen.exitTargetScreen;
                        if (ImGui::InputInt("Exit target screen", &exitScreen)) {
                            screen.exitTargetScreen = (u32)Max(Min(Level::levelMaxScreenCount - 1, exitScreen), 0);
                        }

                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }


            //ImGui::EndDisabled();

            ImGui::End();
        }
	}
}