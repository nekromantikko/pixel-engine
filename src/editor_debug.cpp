#include "editor_debug.h"
#include "editor_util.h"
#include "rendering_util.h"

namespace Editor {
	namespace Debug {

        void DrawNametable(EditorContext* pContext, ImVec2 tablePos, const Rendering::Nametable& nametable) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (int i = 0; i < NAMETABLE_SIZE; i++) {
                s32 x = i % NAMETABLE_WIDTH_TILES;
                s32 y = i / NAMETABLE_WIDTH_TILES;

                u8 tile = nametable.tiles[i];
                ImVec2 tileCoord = Util::GetTileCoord(tile);
                ImVec2 tileStart = Util::TileCoordToTexCoord(tileCoord, 0);
                ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 0);
                ImVec2 pos = ImVec2(tablePos.x + x * 8, tablePos.y + y * 8);
                u8 paletteIndex = Rendering::Util::GetPaletteIndexFromNametableTileAttrib(nametable, x, y);

                drawList->AddImage(pContext->chrTexture[paletteIndex], pos, ImVec2(pos.x + 8, pos.y + 8), tileStart, tileEnd);
            }
        }

        void DrawDebugWindow(EditorContext* pContext, Rendering::RenderContext* pRenderContext) {
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

                    Rendering::Sprite* sprites = Rendering::GetSpritesPtr(pRenderContext, 0);
                    for (int i = 0; i < MAX_SPRITE_COUNT; i++) {
                        const Rendering::Sprite& sprite = sprites[i];
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
                ImVec2 tablePos = Util::DrawTileGrid(pContext, 512, 64);
                Rendering::Nametable* const nametables = Rendering::GetNametablePtr(pRenderContext, 0);
                DrawNametable(pContext, tablePos, nametables[0]);
                ImGui::SameLine();
                tablePos = Util::DrawTileGrid(pContext, 512, 64);
                DrawNametable(pContext, tablePos, nametables[1]);
                ImGui::TreePop();
            }

            ImGui::End();
        }
	}
}