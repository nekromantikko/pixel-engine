#include "editor_chr.h"
#include "editor_util.h"

namespace Editor {
	namespace CHR {
        constexpr s32 renderScale = 3;
        constexpr s32 gridSizeTiles = 16;
        constexpr s32 gridStepPixels = TILE_SIZE * renderScale;
        constexpr s32 gridSizePixels = gridSizeTiles * gridStepPixels;

        void DrawCHRSheet(EditorContext* pContext, bool index) {

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 chrPos = Util::DrawTileGrid(pContext, gridSizePixels, gridSizeTiles);
            drawList->AddImage(pContext->chrTexture[pContext->chrPaletteIndex[index] + index*4], chrPos, ImVec2(chrPos.x + gridSizePixels, chrPos.y + gridSizePixels), ImVec2(0 + 0.5*index, 0), ImVec2(0.5 + 0.5*index, 1));

            // Selection rect
            ImVec2 selectedTilePos = ImVec2(chrPos.x + gridStepPixels * (pContext->chrSelection[index] % 16), chrPos.y + gridStepPixels * (pContext->chrSelection[index] / 16));
            drawList->AddRect(selectedTilePos, ImVec2(selectedTilePos.x + gridStepPixels, selectedTilePos.y + gridStepPixels), IM_COL32(255, 255, 255, 255));

            // Handle selection
            ImGuiIO& io = ImGui::GetIO();
            bool gridClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= chrPos.x && io.MousePos.x < chrPos.x + gridSizePixels && io.MousePos.y >= chrPos.y && io.MousePos.y < chrPos.y + gridSizePixels;
            if (gridClicked) {
                ImVec2 mousePosRelative = ImVec2(io.MousePos.x - chrPos.x, io.MousePos.y - chrPos.y);
                ImVec2 mousePosNormalized = ImVec2((mousePosRelative.x / gridSizePixels) * 0.5f, mousePosRelative.y / gridSizePixels);
                ImVec2 clickedTileCoord = Util::TexCoordToTileCoord(mousePosNormalized);
                u8 clickedTileIndex = Util::GetTileIndex(clickedTileCoord);
                pContext->chrSelection[index] = clickedTileIndex;
            }
        }

		void DrawCHRWindow(EditorContext* pContext) {
            ImGui::Begin("CHR");

            for (int i = 0; i < 8; i++) {
                ImGui::PushID(i);
                if (ImGui::ImageButton("", pContext->paletteTexture, ImVec2(80, 10), ImVec2(0.125 * i, 0), ImVec2(0.125 * (i + 1), 1))) {
                    if (i < 4) {
                        pContext->chrPaletteIndex[0] = i;
                    }
                    else pContext->chrPaletteIndex[1] = i - 4;
                }
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::NewLine();

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            DrawCHRSheet(pContext, 0);
            ImGui::SameLine();
            DrawCHRSheet(pContext, 1);

            ImGui::End();
		}
	}
}