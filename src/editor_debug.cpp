#include "editor_debug.h"
#include "editor_util.h"
#include "rendering_util.h"

namespace Editor {
	namespace Debug {

        void DrawNametable(EditorContext* pContext, ImVec2 tablePos, const Nametable& nametable) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (int i = 0; i < NAMETABLE_SIZE_TILES; i++) {
                s32 x = i % NAMETABLE_WIDTH_TILES;
                s32 y = i / NAMETABLE_WIDTH_TILES;

                u8 tile = nametable.tiles[i];
                ImVec2 tileCoord = Util::GetTileCoord(tile);
                ImVec2 tileStart = Util::TileCoordToTexCoord(tileCoord, 0);
                ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 0);
                ImVec2 pos = ImVec2(tablePos.x + x * 8, tablePos.y + y * 8);
                u8 paletteIndex = Rendering::Util::GetPaletteIndexFromNametableTileAttrib(nametable, x, y);

                drawList->AddImage(pContext->chrTextures[paletteIndex], pos, ImVec2(pos.x + 8, pos.y + 8), tileStart, tileEnd);
            }
        }

        void DrawDebugWindow(EditorContext* pContext) {
            ImGui::Begin("Debug");

            // ImGui::Checkbox("CRT filter", (bool*)&(pRenderSettings->useCRTFilter));

            if (ImGui::TreeNode("Sprites")) {
                ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
                if (ImGui::BeginTable("sprites", 4, flags)) {
                    ImGui::TableSetupColumn("Sprite");
                    ImGui::TableSetupColumn("Pos");
                    ImGui::TableSetupColumn("Tile");
                    ImGui::TableSetupColumn("Attr");
                    ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                    ImGui::TableHeadersRow();

                    Sprite* sprites = Rendering::GetSpritesPtr(0);
                    for (int i = 0; i < MAX_SPRITE_COUNT; i++) {
                        const Sprite& sprite = sprites[i];
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

            if (ImGui::TreeNode("Nametables")) {
                const ImVec2 nametableSizePx = ImVec2(NAMETABLE_WIDTH_TILES * TILE_DIM_PIXELS, NAMETABLE_HEIGHT_TILES * TILE_DIM_PIXELS);
                ImVec2 tablePos = Util::DrawTileGrid(pContext, nametableSizePx, TILE_DIM_PIXELS);
                Nametable* const nametables = Rendering::GetNametablePtr(0);
                DrawNametable(pContext, tablePos, nametables[0]);
                ImGui::SameLine();
                tablePos = Util::DrawTileGrid(pContext, nametableSizePx, TILE_DIM_PIXELS);
                DrawNametable(pContext, tablePos, nametables[1]);
                ImGui::TreePop();
            }

            if (ImGui::Button("Save palette to file")) {
                Rendering::Util::SavePaletteToFile("generated.pal");
            }

            ImGui::End();
        }
	}
}