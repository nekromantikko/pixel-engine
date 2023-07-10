#include "editor_chr.h"
#include "editor_util.h"

namespace Editor {
	namespace CHR {
        constexpr s32 renderScale = 3;
        constexpr s32 gridSizeTiles = 16;
        constexpr s32 gridStepPixels = TILE_SIZE * renderScale;
        constexpr s32 gridSizePixels = gridSizeTiles * gridStepPixels;

		void DrawCHRWindow(EditorContext* pContext) {
            ImGui::Begin("CHR");

            for (int i = 0; i < 8; i++) {
                ImGui::PushID(i);
                if (ImGui::ImageButton("", pContext->paletteTexture, ImVec2(80, 10), ImVec2(0.125 * i, 0), ImVec2(0.125 * (i + 1), 1))) {
                    if (i < 4) {
                        pContext->chrPalette0Index = i;
                    }
                    else pContext->chrPalette1Index = i - 4;
                }
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::NewLine();

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            ImVec2 chrPos = Util::DrawTileGrid(pContext, gridSizePixels, gridSizeTiles);
            drawList->AddImage(pContext->chrTexture[pContext->chrPalette0Index], chrPos, ImVec2(chrPos.x + gridSizePixels, chrPos.y + gridSizePixels), ImVec2(0, 0), ImVec2(0.5, 1));

            ImVec2 selectedTilePos = ImVec2(chrPos.x + gridStepPixels * (pContext->chr0Selection % 16), chrPos.y + gridStepPixels * (pContext->chr0Selection / 16));
            drawList->AddRect(selectedTilePos, ImVec2(selectedTilePos.x + gridStepPixels, selectedTilePos.y + gridStepPixels), IM_COL32(255, 255, 255, 255));

            ImGui::SameLine();

            chrPos = Util::DrawTileGrid(pContext, gridSizePixels, gridSizeTiles);
            drawList->AddImage(pContext->chrTexture[pContext->chrPalette1Index + 4], chrPos, ImVec2(chrPos.x + gridSizePixels, chrPos.y + gridSizePixels), ImVec2(0.5, 0), ImVec2(1, 1));

            selectedTilePos = ImVec2(chrPos.x + gridStepPixels * (pContext->chr1Selection % 16), chrPos.y + gridStepPixels * (pContext->chr1Selection / 16));
            drawList->AddRect(selectedTilePos, ImVec2(selectedTilePos.x + gridStepPixels, selectedTilePos.y + gridStepPixels), IM_COL32(255, 255, 255, 255));

            // Handle selection
            ImGuiIO& io = ImGui::GetIO();
            bool gridClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= chrPos.x && io.MousePos.x < chrPos.x + gridSizePixels && io.MousePos.y >= chrPos.y && io.MousePos.y < chrPos.y + gridSizePixels;
            if (gridClicked) {
                ImVec2 mousePosRelative = ImVec2(io.MousePos.x - chrPos.x, io.MousePos.y - chrPos.y);
                ImVec2 mousePosNormalized = ImVec2((mousePosRelative.x / gridSizePixels) * 0.5f, mousePosRelative.y / gridSizePixels);
                ImVec2 clickedTileCoord = Util::TexCoordToTileCoord(mousePosNormalized);
                u8 clickedTileIndex = Util::GetTileIndex(clickedTileCoord);
                pContext->chr1Selection = clickedTileIndex;
            }

            ImGui::End();
		}
	}
}