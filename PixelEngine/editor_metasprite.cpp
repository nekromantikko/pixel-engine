#include "editor_metasprite.h"
#include "editor_util.h"

namespace Editor {
    constexpr s32 renderScale = 3;
    constexpr s32 gridSizeTiles = 64;
    constexpr s32 gridSizepixels = gridSizeTiles * renderScale;
    constexpr s32 gridStepPixels = TILE_SIZE * renderScale;

	namespace Metasprite {

		void DrawPreviewWindow(EditorContext* pContext, Metasprite* pMetasprite) {
            ImGui::Begin("Metasprite preview");

            ImVec2 gridPos = Util::DrawTileGrid(pContext, gridSizepixels, 8);
            ImVec2 origin = ImVec2(gridPos.x + gridSizepixels / 2, gridPos.y + gridSizepixels / 2);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddLine(ImVec2(origin.x - 10, origin.y), ImVec2(origin.x + 10, origin.y), IM_COL32(200, 200, 200, 255));
            drawList->AddLine(ImVec2(origin.x, origin.y - 10), ImVec2(origin.x, origin.y + 10), IM_COL32(200, 200, 200, 255));

            for (int i = pMetasprite->spriteCount - 1; i >= 0; i--) {
                Rendering::Sprite sprite = pMetasprite->spritesRelativePos[i];
                u8 index = (u8)sprite.tileId;
                ImVec2 tileCoord = Util::GetTileCoord(index);
                ImVec2 tileStart = Util::TileCoordToTexCoord(tileCoord, 1);
                ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 1);
                ImVec2 pos = ImVec2(origin.x + renderScale * sprite.x, origin.y + renderScale * sprite.y);
                bool flipX = sprite.attributes & 0b01000000;
                bool flipY = sprite.attributes & 0b10000000;
                u8 palette = (sprite.attributes & 3) + 4;
                drawList->AddImage(pContext->chrTexture[palette], pos, ImVec2(pos.x + gridStepPixels, pos.y + gridStepPixels), ImVec2(flipX ? tileEnd.x : tileStart.x, flipY ? tileEnd.y : tileStart.y), ImVec2(!flipX ? tileEnd.x : tileStart.x, !flipY ? tileEnd.y : tileStart.y));
            }
            ImGui::End();
		}
	}
}