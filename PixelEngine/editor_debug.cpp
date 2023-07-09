#include "editor_debug.h"
#include "editor_util.h"

namespace Editor {
	namespace Debug {

        void DrawNametable(EditorContext* pContext, ImVec2 tablePos, u8* pNametable) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (int i = 0; i < NAMETABLE_SIZE; i++) {
                s32 x = i % NAMETABLE_WIDTH_TILES;
                s32 y = i / NAMETABLE_WIDTH_TILES;

                u8 tile = pNametable[i];
                ImVec2 tileCoord = Util::GetTileCoord(tile);
                ImVec2 tileStart = Util::TileCoordToTexCoord(tileCoord, 0);
                ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 0);
                ImVec2 pos = ImVec2(tablePos.x + x * 8, tablePos.y + y * 8);

                // Palette from attribs
                s32 xBlock = x / 4;
                s32 yBlock = y / 4;
                s32 smallBlockOffset = (x % 4 / 2) + (y % 4 / 2) * 2;
                s32 blockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
                s32 nametableOffset = NAMETABLE_ATTRIBUTE_OFFSET + blockIndex;
                u8 attribute = pNametable[nametableOffset];
                u8 paletteIndex = (attribute >> (smallBlockOffset * 2)) & 0b11;

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

                    static Rendering::Sprite sprites[MAX_SPRITE_COUNT];
                    Rendering::ReadSprites(pRenderContext, MAX_SPRITE_COUNT, 0, sprites);
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
                    if (ImGui::ImageButton("", pContext->paletteTexture, ImVec2(80, 10), ImVec2(0.125 * i, 0), ImVec2(0.125 * (i + 1), 1)))
                        pContext->paletteIndex = i;
                    ImGui::PopID();
                    ImGui::SameLine();
                }
                ImGui::NewLine();

                const u32 chrSize = 384;

                ImDrawList* drawList = ImGui::GetWindowDrawList();

                ImVec2 chrPos = Util::DrawTileGrid(pContext, chrSize, 16);
                //ImGui::Image(debugChrTexture, ImVec2(chrSize, chrSize), ImVec2(0, 0), ImVec2(0.5, 1));
                drawList->AddImage(pContext->chrTexture[pContext->paletteIndex], chrPos, ImVec2(chrPos.x + chrSize, chrPos.y + chrSize), ImVec2(0, 0), ImVec2(0.5, 1));
                ImGui::SameLine();
                chrPos = Util::DrawTileGrid(pContext, chrSize, 16);
                //ImGui::Image(debugChrTexture, ImVec2(chrSize, chrSize), ImVec2(0.5, 0), ImVec2(1, 1));
                drawList->AddImage(pContext->chrTexture[pContext->paletteIndex], chrPos, ImVec2(chrPos.x + chrSize, chrPos.y + chrSize), ImVec2(0.5, 0), ImVec2(1, 1));
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Nametables")) {
                ImVec2 tablePos = Util::DrawTileGrid(pContext, 512, 64);
                static u8 nametable[NAMETABLE_SIZE];
                Rendering::ReadNametable(pRenderContext, 0, NAMETABLE_SIZE, 0, nametable);
                DrawNametable(pContext, tablePos, nametable);
                ImGui::SameLine();
                tablePos = Util::DrawTileGrid(pContext, 512, 64);
                Rendering::ReadNametable(pRenderContext, 1, NAMETABLE_SIZE, 0, nametable);
                DrawNametable(pContext, tablePos, nametable);
                ImGui::TreePop();
            }

            ImGui::End();
        }
	}
}