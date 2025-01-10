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

        constexpr u32 actorTypeCount = 2;
        constexpr const char* actorTypeNames[actorTypeCount] = { "None", "PStart" };

        ActorType selectedActorType = ACTOR_NONE;
        bool actorMode = false;

        static void DrawActor(const ImVec2 contentTopLeft, const ImVec2 contentBtmRight, const ImVec2 actorTopLeft, const r32 metatileDrawSize, const ActorType type, const u8 opacity) {
            if (type == ACTOR_NONE) {
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

        static void DrawActors(const Level* pLevel, const Viewport* pViewport, const ImVec2 topLeft, const ImVec2 btmRight, const r32 tileDrawSize) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const r32 metatileDrawSize = tileDrawSize * Tileset::metatileWorldSize;

            const u32 leftScreenIndex = pViewport->x / LEVEL_SCREEN_WIDTH_TILES;
            const u32 rightScreenIndex = (pViewport->x + pViewport->w) / LEVEL_SCREEN_WIDTH_TILES;

            const r32 leftScreenOrigin = leftScreenIndex * LEVEL_SCREEN_WIDTH_TILES;

            for (u32 i = leftScreenIndex; i <= rightScreenIndex; i++) {
                const r32 screenLeft = i * LEVEL_SCREEN_WIDTH_TILES;
                const r32 screenLeftViewportCoords = (screenLeft - pViewport->x) * tileDrawSize;
                const Screen& screen = pLevel->screens[i];

                static char screenLabelText[16];
                snprintf(screenLabelText, 16, "Screen #%d", i);

                drawList->AddLine(ImVec2(topLeft.x + screenLeftViewportCoords, topLeft.y), ImVec2(topLeft.x + screenLeftViewportCoords, btmRight.y), IM_COL32(255, 255, 255, 255), 1.0f);
                drawList->AddText(ImVec2(topLeft.x + screenLeftViewportCoords + tileDrawSize, btmRight.y - 2 * tileDrawSize), IM_COL32(255, 255, 255, 255), screenLabelText);

                for (u32 y = 0; y < LEVEL_SCREEN_HEIGHT_METATILES; y++) {
                    for (u32 x = 0; x < LEVEL_SCREEN_WIDTH_METATILES; x++) {
                        ImVec2 actorTopLeft = ImVec2(topLeft.x + screenLeftViewportCoords + x * metatileDrawSize, topLeft.y - pViewport->y * tileDrawSize + y * metatileDrawSize);
                        ImVec2 actorBtmRight = ImVec2(actorTopLeft.x + metatileDrawSize, actorTopLeft.y + metatileDrawSize);

                        if (actorBtmRight.x < topLeft.x || actorBtmRight.y < topLeft.y || actorTopLeft.x >= btmRight.x || actorTopLeft.y >= btmRight.y) {
                            continue;
                        }

                        const u32 tileIndex = x + LEVEL_SCREEN_WIDTH_METATILES * y;
                        const ActorType type = screen.tiles[tileIndex].actorType;

                        DrawActor(topLeft, btmRight, actorTopLeft, metatileDrawSize, type, actorMode ? 127 : 15);
                    }
                }
            }
        }

		void DrawGameWindow(EditorContext* pEditorContext, Rendering::RenderContext* pRenderContext) {
            ImGui::Begin("Game");
            bool editMode = Game::IsPaused();
            if (ImGui::Button(editMode ? "Play mode" : "Edit mode")) {
                Game::SetPaused(!editMode);
                editMode = !editMode;
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 topLeft = ImGui::GetCursorScreenPos();
            static const r32 aspectRatio = (r32)VIEWPORT_WIDTH_TILES / VIEWPORT_HEIGHT_TILES;
            const r32 contentWidth = ImGui::GetContentRegionAvail().x;
            const r32 contentHeight = contentWidth / aspectRatio;
            const r32 renderScale = contentWidth / (VIEWPORT_WIDTH_TILES * TILE_SIZE);
            ImVec2 btmRight = ImVec2(topLeft.x + contentWidth, topLeft.y + contentHeight);
            drawList->AddImage(pEditorContext->gameViewTexture, topLeft, btmRight);

            if (editMode) {
                Level* pLevel = Game::GetLevel();
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
                    SaveLevel(pLevel, "assets/test.lev");
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert changes")) {
                    LoadLevel(pLevel, "assets/test.lev");
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

                                    const ImVec2 metatileWorldPos = ImVec2(selectionTopLeft.x + x * Tileset::metatileWorldSize, selectionTopLeft.y + y * Tileset::metatileWorldSize);

                                    const u32 screenIndex = ((u32)metatileWorldPos.x) / LEVEL_SCREEN_WIDTH_TILES;
                                    const u32 screenRelativeX = ((u32)metatileWorldPos.x) % LEVEL_SCREEN_WIDTH_TILES;
                                    const u32 screenRelativeY = ((u32)metatileWorldPos.y) % LEVEL_SCREEN_HEIGHT_TILES;
                                    const u32 screenTileIndex = (screenRelativeY / Tileset::metatileWorldSize) * LEVEL_SCREEN_WIDTH_METATILES + (screenRelativeX / Tileset::metatileWorldSize);
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

                            const u32 screenIndex = ((u32)hoveredMetatileWorldPos.x) / LEVEL_SCREEN_WIDTH_TILES;

                            DrawActor(topLeft, btmRight, metatileInPixelCoords, metatileDrawSize, selectedActorType, 63);
                            drawList->AddRect(metatileInPixelCoords, ImVec2(metatileInPixelCoords.x + metatileDrawSize, metatileInPixelCoords.y + metatileDrawSize), IM_COL32(255, 255, 255, 255));

                            // Paint actors
                            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                if (screenIndex < pLevel->screenCount) {
                                    const u32 screenRelativeX = ((u32)hoveredMetatileWorldPos.x) % LEVEL_SCREEN_WIDTH_TILES;
                                    const u32 screenRelativeY = ((u32)hoveredMetatileWorldPos.y) % LEVEL_SCREEN_HEIGHT_TILES;
                                    const u32 screenTileIndex = (screenRelativeY / Tileset::metatileWorldSize) * LEVEL_SCREEN_WIDTH_METATILES + (screenRelativeX / Tileset::metatileWorldSize);

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
                                    const ImVec2 metatileWorldPos = ImVec2(clipboardTopLeft.x + x* Tileset::metatileWorldSize, clipboardTopLeft.y + y* Tileset::metatileWorldSize);
                                    const ImVec2 metatileInViewportCoords = ImVec2(metatileWorldPos.x - pViewport->x, metatileWorldPos.y - pViewport->y);
                                    const ImVec2 metatileInPixelCoords = ImVec2(metatileInViewportCoords.x * tileDrawSize + topLeft.x, metatileInViewportCoords.y * tileDrawSize + topLeft.y);
                                    const u8 metatileIndex = pEditorContext->levelClipboard[clipboardIndex];

                                    Util::DrawMetatile(pEditorContext, metatileIndex, metatileInPixelCoords, tileDrawSize, IM_COL32(255, 255, 255, 127));

                                    // Paint metatiles
                                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                        u32 screenIndex = ((u32)metatileWorldPos.x) / LEVEL_SCREEN_WIDTH_TILES;
                                        if (screenIndex >= pLevel->screenCount) {
                                            continue;
                                        }

                                        const u32 screenRelativeX = ((u32)metatileWorldPos.x) % LEVEL_SCREEN_WIDTH_TILES;
                                        const u32 screenRelativeY = ((u32)metatileWorldPos.y) % LEVEL_SCREEN_HEIGHT_TILES;
                                        const u32 screenTileIndex = (screenRelativeY / Tileset::metatileWorldSize) * LEVEL_SCREEN_WIDTH_METATILES + (screenRelativeX / Tileset::metatileWorldSize);

                                        pLevel->screens[screenIndex].tiles[screenTileIndex].metatile = metatileIndex;
                                        const u32 nametableIndex = screenIndex % NAMETABLE_COUNT;
                                        Tileset::CopyMetatileToNametable(&pNametables[nametableIndex], screenRelativeX, (u16)metatileWorldPos.y, metatileIndex);
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
                    selectedActorType = (ActorType)i;
                }
            }

            ImGui::End();
        }
	}
}