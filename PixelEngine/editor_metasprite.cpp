#include "editor_metasprite.h"
#include "editor_util.h"

namespace Editor {
    constexpr s32 renderScale = 4;
    constexpr s32 gridSizeTiles = 8;
    constexpr s32 gridStepPixels = TILE_SIZE * renderScale;
    constexpr s32 gridSizePixels = gridSizeTiles * gridStepPixels;

    static ImVector<s32> selection;
    bool selectionLocked = false;

	namespace Metasprite {

		void DrawPreviewWindow(EditorContext* pContext, Metasprite* pMetasprite) {
            ImGui::Begin("Metasprite preview");

            static bool gridFocused = false;
            static bool dragging = false;
            static ImVec2 dragStartPos = ImVec2(0, 0);
            static ImVec2 dragDelta = ImVec2(0, 0);

            ImVec2 gridPos = Util::DrawTileGrid(pContext, gridSizePixels, 8, &gridFocused);
            ImVec2 origin = ImVec2(gridPos.x + gridSizePixels / 2, gridPos.y + gridSizePixels / 2);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddLine(ImVec2(origin.x - 10, origin.y), ImVec2(origin.x + 10, origin.y), IM_COL32(200, 200, 200, 255));
            drawList->AddLine(ImVec2(origin.x, origin.y - 10), ImVec2(origin.x, origin.y + 10), IM_COL32(200, 200, 200, 255));

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && gridFocused) {
                dragging = true;
                dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                
                // Pixel snap
                dragDelta.x = round(dragDelta.x / renderScale) * renderScale;
                dragDelta.y = round(dragDelta.y / renderScale) * renderScale;
            }
            s32 trySelect = (gridFocused && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ? -2 : -1; // -2 = deselect all (Clicked outside tiles)
            ImGuiIO& io = ImGui::GetIO();

            for (s32 i = pMetasprite->spriteCount - 1; i >= 0; i--) {
                Rendering::Sprite& sprite = pMetasprite->spritesRelativePos[i];
                u8 index = (u8)sprite.tileId;
                ImVec2 tileCoord = Util::GetTileCoord(index);
                ImVec2 tileStart = Util::TileCoordToTexCoord(tileCoord, 1);
                ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 1);
                ImVec2 pos = ImVec2(origin.x + renderScale * sprite.x, origin.y + renderScale * sprite.y);
                bool flipX = sprite.attributes & 0b01000000;
                bool flipY = sprite.attributes & 0b10000000;
                u8 palette = (sprite.attributes & 3) + 4;

                // Select sprite by clicking (Topmost sprite gets selected)
                bool spriteClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= pos.x && io.MousePos.x < pos.x + gridStepPixels && io.MousePos.y >= pos.y && io.MousePos.y < pos.y + gridStepPixels;
                if (spriteClicked) {
                    trySelect = i;
                }

                bool selected = selection.contains(i);
                // Move sprite if dragged
                ImVec2 posWithDrag = selected ? ImVec2(pos.x + dragDelta.x, pos.y + dragDelta.y) : pos;

                drawList->AddImage(pContext->chrTexture[palette], posWithDrag, ImVec2(posWithDrag.x + gridStepPixels, posWithDrag.y + gridStepPixels), ImVec2(flipX ? tileEnd.x : tileStart.x, flipY ? tileEnd.y : tileStart.y), ImVec2(!flipX ? tileEnd.x : tileStart.x, !flipY ? tileEnd.y : tileStart.y));


                // Commit drag
                if (selected && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    ImVec2 deltaInTileCoord = ImVec2(dragDelta.x / renderScale, dragDelta.y / renderScale);
                    sprite.x += deltaInTileCoord.x;
                    sprite.y += deltaInTileCoord.y;
                }

                // Draw selection box
                if (selected) {
                    drawList->AddRect(posWithDrag, ImVec2(posWithDrag.x + gridStepPixels, posWithDrag.y + gridStepPixels), IM_COL32(255, 255, 255, 255));
                }
            }

            if (trySelect >= 0 && !selectionLocked && !dragging) {
                if (ImGui::IsKeyDown(ImGuiKey_ModCtrl))
                {
                    if (selection.contains(trySelect))
                        selection.find_erase_unsorted(trySelect);
                    else
                        selection.push_back(trySelect);
                }
                else if (!selection.contains(trySelect))
                {
                    selection.clear();
                    selection.push_back(trySelect);
                }
            }
            else if (trySelect == -2 && !ImGui::IsKeyDown(ImGuiKey_ModCtrl) && !selectionLocked) {
                selection.clear();
            }

            // Reset drag delta when mouse released
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                dragging = false;
                dragDelta = ImVec2(0, 0);
            }

            ImGui::End();
		}

        void DrawSpriteListWindow(EditorContext* pContext, Metasprite* pMetasprite) {
            ImGui::Begin("Metasprite");

            ImGui::Checkbox("Lock selection", &selectionLocked);

            if (ImGui::Button("+")) {
                pMetasprite->spritesRelativePos[pMetasprite->spriteCount++] = {
                    0,
                    0,
                    pContext->chr1Selection,
                    (u32)pContext->chrPalette1Index,
                };
            }
            ImGui::SameLine();
            if (ImGui::Button("-") && pMetasprite->spriteCount > 1) {
                pMetasprite->spritesRelativePos[--pMetasprite->spriteCount] = {
                    0,
                    0,
                    0,
                    0,
                };
            }

            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
            ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
            if (ImGui::BeginTable("sprites", 4, flags)) {
                ImGui::TableSetupColumn("Sprite");
                ImGui::TableSetupColumn("Pos");
                ImGui::TableSetupColumn("Tile");
                ImGui::TableSetupColumn("Attr");
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();

                for (int i = 0; i < pMetasprite->spriteCount; i++) {
                    Rendering::Sprite& sprite = pMetasprite->spritesRelativePos[i];
                    char labelStr[5];
                    snprintf(labelStr, 5, "%04d", i);
                    bool selected = selection.contains(i);

                    ImGui::PushID(i);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    // ImGui::Text("%04d", i);
                    if (ImGui::Selectable(labelStr, selected, selectableFlags, ImVec2(0, 0)) && !selectionLocked) {

                        if (ImGui::IsKeyDown(ImGuiKey_ModCtrl))
                        {
                            if (selected)
                                selection.find_erase_unsorted(i);
                            else
                                selection.push_back(i);
                        }
                        else
                        {
                            selection.clear();
                            selection.push_back(i);
                        }

                    }
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

            ImGui::End();
        }

        void DrawSpriteEditor(EditorContext* pContext, Metasprite* pMetasprite) {
            ImGui::Begin("Sprite editor");

            if (selection.empty()) {
                ImGui::TextUnformatted("No sprite selected");
            }
            else if (selection.size() > 1) {
                ImGui::TextUnformatted("Multiple sprites selected");
            }
            else {
                s32& spriteIndex = selection[0];
                Rendering::Sprite& sprite = pMetasprite->spritesRelativePos[spriteIndex];
                s32 index = (s32)sprite.tileId;

                bool flipX = sprite.attributes & 0b01000000;
                bool flipY = sprite.attributes & 0b10000000;
                s32 palette = sprite.attributes & 3;

                ImGui::InputInt("Tile", &index);
                ImGui::SameLine();
                if (ImGui::Button("CHR")) {
                    index = (s32)pContext->chr1Selection;
                }
                ImGui::SliderInt("Palette", &palette, 0, 3);
                ImGui::Checkbox("Flip horizontal", &flipX);
                ImGui::SameLine();
                ImGui::Checkbox("Flip vertical", &flipY);

                sprite.tileId = (u8)index;
                sprite.attributes = (sprite.attributes & ~0b00000011) | palette;
                sprite.attributes = (sprite.attributes & ~0b01000000) | (flipX << 6);
                sprite.attributes = (sprite.attributes & ~0b10000000) | (flipY << 7);

                ImGui::BeginDisabled(spriteIndex == 0);
                if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
                    // Swap sprite above this
                    Rendering::Sprite movedSprite = sprite;
                    pMetasprite->spritesRelativePos[spriteIndex] = pMetasprite->spritesRelativePos[spriteIndex-1];
                    pMetasprite->spritesRelativePos[--spriteIndex] = movedSprite;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(spriteIndex == pMetasprite->spriteCount-1);
                if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
                    // Swap sprite below this
                    Rendering::Sprite movedSprite = sprite;
                    pMetasprite->spritesRelativePos[spriteIndex] = pMetasprite->spritesRelativePos[spriteIndex + 1];
                    pMetasprite->spritesRelativePos[++spriteIndex] = movedSprite;
                }
                ImGui::EndDisabled();

                if (ImGui::Button("Duplicate")) {
                    spriteIndex = pMetasprite->spriteCount++;
                    pMetasprite->spritesRelativePos[spriteIndex] = sprite;
                }
            }

            ImGui::End();
        }
	}
}