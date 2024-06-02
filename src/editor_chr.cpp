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
            ImVec2 chrPos = Util::DrawTileGrid(pContext, gridSizePixels, gridSizeTiles, (u32*)&pContext->chrSelection[index]);
            drawList->AddImage(pContext->chrTexture[pContext->chrPaletteIndex[index] + index*4], chrPos, ImVec2(chrPos.x + gridSizePixels, chrPos.y + gridSizePixels), ImVec2(0 + 0.5*index, 0), ImVec2(0.5 + 0.5*index, 1));
            Util::DrawTileGridSelection(pContext, chrPos, gridSizePixels, gridSizeTiles, pContext->chrSelection[index]);
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

            DrawCHRSheet(pContext, 0);
            ImGui::SameLine();
            DrawCHRSheet(pContext, 1);

            ImGui::End();
		}
	}
}