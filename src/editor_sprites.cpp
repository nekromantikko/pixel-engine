#include "editor_sprites.h"
#include "editor_util.h"
#include <cmath>
#include <stdio.h>

namespace Editor {
	namespace Sprites {

        constexpr s32 renderScale = 4;
        constexpr s32 gridSizeTiles = 8;
        constexpr s32 gridStepPixels = TILE_SIZE * renderScale;
        constexpr s32 gridSizePixels = gridSizeTiles * gridStepPixels;

        s32 selection = 0;
        s32 copyFrom = 0;
        bool showColliderPreview = true;
        
        ImVector<s32> spriteSelection;
        bool selectionLocked = false;

        constexpr u32 colliderTypeCount = 4;
        constexpr const char* colliderTypeNames[colliderTypeCount] = { "Point", "Box", "Circle", "Capsule" };

		void DrawPreviewWindow(EditorContext* pContext) {
            ImGui::Begin("Metasprite preview");

            static bool gridFocused = false;
            static bool dragging = false;
            static ImVec2 dragDelta = ImVec2(0, 0);

            ImVec2 gridPos = Util::DrawTileGrid(pContext, gridSizePixels, 8, nullptr, &gridFocused);
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

            Metasprite::Metasprite& metasprite = Metasprite::GetMetaspritesPtr()[selection];

            for (s32 i = metasprite.spriteCount - 1; i >= 0; i--) {
                Rendering::Sprite& sprite = metasprite.spritesRelativePos[i];
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

                bool selected = spriteSelection.contains(i);
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
                    if (spriteSelection.contains(trySelect))
                        spriteSelection.find_erase_unsorted(trySelect);
                    else
                        spriteSelection.push_back(trySelect);
                }
                else if (!spriteSelection.contains(trySelect))
                {
                    spriteSelection.clear();
                    spriteSelection.push_back(trySelect);
                }
            }
            else if (trySelect == -2 && !ImGui::IsKeyDown(ImGuiKey_ModCtrl) && !selectionLocked) {
                spriteSelection.clear();
            }

            // Reset drag delta when mouse released
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                dragging = false;
                dragDelta = ImVec2(0, 0);
            }

            // Draw collider visualization
            if (showColliderPreview) {
                for (u32 i = 0; i < metasprite.colliderCount; i++) {

                    Collision::Collider& collider = metasprite.colliders[i];

                    ImVec2 colliderPos = ImVec2(origin.x + gridStepPixels * collider.xOffset, origin.y + gridStepPixels * collider.yOffset);

                    switch (collider.type) {
                    case Collision::ColliderPoint: {
                        drawList->AddLine(ImVec2(colliderPos.x - 10, colliderPos.y), ImVec2(colliderPos.x + 10, colliderPos.y), IM_COL32(0, 255, 0, 255));
                        drawList->AddLine(ImVec2(colliderPos.x, colliderPos.y - 10), ImVec2(colliderPos.x, colliderPos.y + 10), IM_COL32(0, 255, 0, 255));
                        break;
                    }
                    case Collision::ColliderCircle: {
                        drawList->AddCircleFilled(colliderPos, collider.width/2 * gridStepPixels, IM_COL32(0, 255, 0, 80));
                        break;
                    }
                    case Collision::ColliderBox: {
                        ImVec2 topLeft = ImVec2(colliderPos.x - gridStepPixels * collider.width / 2, colliderPos.y - gridStepPixels * collider.height / 2);
                        ImVec2 btmRight = ImVec2(colliderPos.x + gridStepPixels * collider.width / 2, colliderPos.y + gridStepPixels * collider.height / 2);

                        drawList->AddRectFilled(topLeft, btmRight, IM_COL32(0, 255, 0, 80));
                        break;
                    }
                    case Collision::ColliderCapsule: {
                        ImVec2 topLeft = ImVec2(colliderPos.x - gridStepPixels * collider.width/2, colliderPos.y + gridStepPixels * collider.height / 2);
                        ImVec2 btmRight = ImVec2(colliderPos.x + gridStepPixels * collider.width/2, colliderPos.y - gridStepPixels * collider.height / 2);
                        drawList->AddRectFilled(topLeft, btmRight, IM_COL32(0, 255, 0, 80));

                        drawList->PushClipRect(ImVec2(topLeft.x, btmRight.y - (collider.width / 2) * gridStepPixels), ImVec2(btmRight.x, btmRight.y));
                        drawList->AddCircleFilled(ImVec2(colliderPos.x, colliderPos.y - collider.height/2 * gridStepPixels), collider.width/2 * gridStepPixels, IM_COL32(0, 255, 0, 80));
                        drawList->PopClipRect();

                        drawList->PushClipRect(ImVec2(topLeft.x, topLeft.y), ImVec2(btmRight.x, topLeft.y + (collider.width / 2) * gridStepPixels));
                        drawList->AddCircleFilled(ImVec2(colliderPos.x, colliderPos.y + collider.height/2 * gridStepPixels), collider.width/2 * gridStepPixels, IM_COL32(0, 255, 0, 80));
                        drawList->PopClipRect();

                        break;
                    }
                    default:
                        break;
                    }
                }
            }

            ImGui::End();
		}

        void DrawMetaspriteWindow(EditorContext* pContext) {
            ImGui::Begin("Metasprite");

            if (ImGui::Button("Save")) {
                Metasprite::SaveMetasprites("assets/meta.spr");
            }
            ImGui::SameLine();
            if (ImGui::Button("Revert changes")) {
                Metasprite::LoadMetasprites("assets/meta.spr");
            }

            Metasprite::Metasprite* pMetasprites = Metasprite::GetMetaspritesPtr();
            if (ImGui::BeginCombo("Metasprite", pMetasprites[selection].name))
            {
                for (u32 i = 0; i < Metasprite::maxMetaspriteCount; i++)
                {
                    ImGui::PushID(i);
                    const bool selected = selection == i;
                    if (ImGui::Selectable(pMetasprites[i].name, selected)) {
                        selection = i;
                    }

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            Metasprite::Metasprite& metasprite = Metasprite::GetMetaspritesPtr()[selection];
            ImGui::InputText("Name", metasprite.name, Metasprite::metaspriteMaxNameLength);

            if (ImGui::TreeNode("Sprites")) {

                ImGui::Checkbox("Lock selection", &selectionLocked);

                ImGui::BeginDisabled(metasprite.spriteCount == Metasprite::metaspriteMaxSpriteCount);
                if (ImGui::Button("+")) {
                    metasprite.spritesRelativePos[metasprite.spriteCount++] = {
                        0,
                        0,
                        pContext->chrSelection[1],
                        (u32)pContext->chrPaletteIndex[1],
                    };
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(metasprite.spriteCount == 0);
                if (ImGui::Button("-")) {
                    metasprite.spritesRelativePos[--metasprite.spriteCount] = {
                        0,
                        0,
                        0,
                        0,
                    };
                }
                ImGui::EndDisabled();

                ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
                ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                if (ImGui::BeginTable("sprites", 4, flags)) {
                    ImGui::TableSetupColumn("Sprite");
                    ImGui::TableSetupColumn("Pos");
                    ImGui::TableSetupColumn("Tile");
                    ImGui::TableSetupColumn("Attr");
                    ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < metasprite.spriteCount; i++) {
                        Rendering::Sprite& sprite = metasprite.spritesRelativePos[i];
                        char labelStr[5];
                        snprintf(labelStr, 5, "%04d", i);
                        bool selected = spriteSelection.contains(i);

                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        // ImGui::Text("%04d", i);
                        if (ImGui::Selectable(labelStr, selected, selectableFlags, ImVec2(0, 0)) && !selectionLocked) {

                            if (ImGui::IsKeyDown(ImGuiKey_ModCtrl))
                            {
                                if (selected)
                                    spriteSelection.find_erase_unsorted(i);
                                else
                                    spriteSelection.push_back(i);
                            }
                            else
                            {
                                spriteSelection.clear();
                                spriteSelection.push_back(i);
                            }

                            if (selected) {
                                ImGui::SetItemDefaultFocus();
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

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Colliders")) {

                ImGui::Checkbox("Show collider preview", &showColliderPreview);

                ImGui::BeginDisabled(metasprite.colliderCount == Metasprite::metaspriteMaxColliderCount);
                if (ImGui::Button("+")) {
                    metasprite.colliders[metasprite.colliderCount++] = Collision::Collider{};
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(metasprite.colliderCount == 0);
                if (ImGui::Button("-") && metasprite.colliderCount > 1) {
                    metasprite.colliders[--metasprite.colliderCount] = Collision::Collider{};;
                }
                ImGui::EndDisabled();

                for (u32 i = 0; i < metasprite.colliderCount; i++) {
                    if (ImGui::TreeNode(&metasprite.colliders[i], "%d", i)) {

                        Collision::Collider& collider = metasprite.colliders[i];
                        ImGui::SliderInt("Type", (s32*)&collider.type, 0, colliderTypeCount - 1, colliderTypeNames[collider.type]);

                        r32 offset[2] = { collider.xOffset, collider.yOffset };

                        if (ImGui::InputFloat2("Offset", offset)) {
                            collider.xOffset = offset[0];
                            collider.yOffset = offset[1];
                        }

                        switch (collider.type) {
                        case Collision::ColliderPoint:
                            break;
                        case Collision::ColliderCircle: {
                            r32 radius = collider.width / 2;
                            if (ImGui::InputFloat("Radius", &radius, 0.125f, 0.0625f)) {
                                collider.width = max(0.0f, radius*2);
                            }
                            break;
                        }
                        case Collision::ColliderBox: {
                            r32 width = collider.width;
                            if (ImGui::InputFloat("Width", &width, 0.125f, 0.0625f)) {
                                collider.width = max(0.0f, width);
                            }

                            r32 height = collider.height;
                            if (ImGui::InputFloat("Height", &height, 0.125f, 0.0625f)) {
                                collider.height = max(0.0f, height);
                            }

                            break;
                        }
                        case Collision::ColliderCapsule: {
                            r32 radius = collider.width / 2;
                            if (ImGui::InputFloat("Radius", &radius, 0.125f, 0.0625f)) {
                                collider.width = max(0.0f, radius * 2);
                            }

                            r32 height = collider.height;
                            if (ImGui::InputFloat("Height", &height, 0.125f, 0.0625f)) {
                                collider.height = max(0.0f, height);
                            }

                            break;
                        }
                        default:
                            break;
                        }

                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }


            if (ImGui::BeginCombo("Copy from", pMetasprites[copyFrom].name))
            {
                for (u32 i = 0; i < Metasprite::maxMetaspriteCount; i++)
                {
                    ImGui::PushID(i);
                    const bool selected = copyFrom == i;
                    if (ImGui::Selectable(pMetasprites[i].name, copyFrom)) {
                        copyFrom = i;
                    }

                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(selection == copyFrom);
            if (ImGui::Button("Copy")) {
                Metasprite::Metasprite& selectedSprite = pMetasprites[selection];
                Metasprite::Metasprite& copyFromSprite = pMetasprites[copyFrom];

                selectedSprite.spriteCount = copyFromSprite.spriteCount;
                selectedSprite.colliderCount = copyFromSprite.colliderCount;
                
                memcpy(selectedSprite.spritesRelativePos, copyFromSprite.spritesRelativePos, Metasprite::metaspriteMaxSpriteCount * sizeof(Rendering::Sprite));
                memcpy(selectedSprite.colliders, copyFromSprite.colliders, Metasprite::metaspriteMaxColliderCount * sizeof(Collision::Collider));
            }
            ImGui::EndDisabled();

            ImGui::End();
        }

        bool TrySwap(Metasprite::Metasprite* pMetasprite, ImVector<s32>& selection, s32 i, s32 step) {
            s32& spriteIndex = selection[i];
            s32 nextSpriteIndex = spriteIndex + step;

            if (selection.contains(nextSpriteIndex)) {
                return false;
            }

            Rendering::Sprite& sprite = pMetasprite->spritesRelativePos[spriteIndex];
            Rendering::Sprite movedSprite = sprite;
            pMetasprite->spritesRelativePos[spriteIndex] = pMetasprite->spritesRelativePos[spriteIndex + step];
            spriteIndex += step;
            pMetasprite->spritesRelativePos[spriteIndex] = movedSprite;

            return true;
        }

        void SwapMultiple(Metasprite::Metasprite* pMetasprite, ImVector<s32>& selection, s32 step) {
            ImVector<s32> alreadyMoved = {};
            while (alreadyMoved.size() < selection.size()) {
                for (u32 i = 0; i < selection.size(); i++) {
                    if (alreadyMoved.contains(i))
                        continue;

                    if (TrySwap(pMetasprite, selection, i, step)) {
                        alreadyMoved.push_back(i);
                    }
                }
            }
        }

        void DrawSpriteEditor(EditorContext* pContext) {
            ImGui::Begin("Sprite editor");

            Metasprite::Metasprite& metasprite = Metasprite::GetMetaspritesPtr()[selection];

            if (spriteSelection.empty()) {
                ImGui::TextUnformatted("No sprite selected");
            }
            else if (spriteSelection.size() > 1) {
                ImGui::TextUnformatted("Multiple sprites selected");

                s32 minSpriteIndex = Metasprite::metaspriteMaxSpriteCount;
                s32 maxSpriteIndex = 0;
                
                for (u32 i = 0; i < spriteSelection.size(); i++) {
                    minSpriteIndex = min(minSpriteIndex, spriteSelection[i]);
                    maxSpriteIndex = max(maxSpriteIndex, spriteSelection[i]);
                }

                ImGui::BeginDisabled(minSpriteIndex == 0);
                if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
                    SwapMultiple(&metasprite, spriteSelection, -1);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(maxSpriteIndex == metasprite.spriteCount - 1);
                if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
                    ImVector<s32> alreadyMoved = {};
                    SwapMultiple(&metasprite, spriteSelection, 1);
                }
                ImGui::EndDisabled();

                if (ImGui::Button("Duplicate")) {
                    for (u32 i = 0; i < spriteSelection.size(); i++) {
                        s32& index = spriteSelection[i];
                        Rendering::Sprite sprite = metasprite.spritesRelativePos[index];
                        index = metasprite.spriteCount++;
                        metasprite.spritesRelativePos[index] = sprite;
                    }
                }
            }
            else {
                s32& spriteIndex = spriteSelection[0];
                Rendering::Sprite& sprite = metasprite.spritesRelativePos[spriteIndex];
                s32 index = (s32)sprite.tileId;

                bool flipX = sprite.attributes & 0b01000000;
                bool flipY = sprite.attributes & 0b10000000;
                s32 palette = sprite.attributes & 3;

                ImGui::InputInt("Tile", &index);
                ImGui::PushID(0); // Buttons with same label need ID
                ImGui::SameLine();
                if (ImGui::Button("CHR")) {
                    index = (s32)pContext->chrSelection[1];
                }
                ImGui::PopID();

                ImGui::SliderInt("Palette", &palette, 0, 3);
                ImGui::SameLine();
                ImGui::PushID(1);
                if (ImGui::Button("CHR")) {
                    palette = (s32)pContext->chrPaletteIndex[1];
                }
                ImGui::PopID();

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
                    metasprite.spritesRelativePos[spriteIndex] = metasprite.spritesRelativePos[spriteIndex-1];
                    metasprite.spritesRelativePos[--spriteIndex] = movedSprite;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(spriteIndex == metasprite.spriteCount-1);
                if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
                    // Swap sprite below this
                    Rendering::Sprite movedSprite = sprite;
                    metasprite.spritesRelativePos[spriteIndex] = metasprite.spritesRelativePos[spriteIndex + 1];
                    metasprite.spritesRelativePos[++spriteIndex] = movedSprite;
                }
                ImGui::EndDisabled();

                if (ImGui::Button("Duplicate")) {
                    spriteIndex = metasprite.spriteCount++;
                    metasprite.spritesRelativePos[spriteIndex] = sprite;
                }
            }

            ImGui::End();
        }
	}
}