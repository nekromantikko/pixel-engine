#include "editor_collision.h"
#include "editor_util.h"
#include "collision.h"

using namespace Collision;

namespace Editor {
	namespace Collision {
		constexpr r32 tilePreviewSize = 64;
		constexpr r32 pixelSize = tilePreviewSize / TILE_SIZE;

		constexpr u32 bgCollisionTypeCount = 5;
		constexpr const char* bgCollisionTypeNames[bgCollisionTypeCount] = { "TileEmpty", "TileSolid", "TilePassThrough", "TileJumpThrough", "TilePassThroughFlip" };

		void DrawCollisionEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext) {
			TileCollision* bgCollision = GetBgCollisionPtr();

			ImGui::Begin("Background Tile Collision");

			u8 selectedTileIndex = pContext->chrSelection[0];
			ImGui::Text("0x%02x", selectedTileIndex);

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 tilePos = Util::DrawTileGrid(pContext, tilePreviewSize, 1);
			ImVec2 tileCoord = Util::GetTileCoord(selectedTileIndex);
			ImVec2 tileStart = Util::TileCoordToTexCoord(tileCoord, 0);
			ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 0);
			drawList->AddImage(pContext->chrTexture[pContext->chrPaletteIndex[0]], tilePos, ImVec2(tilePos.x + tilePreviewSize, tilePos.y + tilePreviewSize), tileStart, tileEnd);

			TileCollision& tileCollision = bgCollision[selectedTileIndex];
			ImGui::SliderInt("Type", (s32*)&tileCollision.type, 0, bgCollisionTypeCount-1, bgCollisionTypeNames[tileCollision.type]);

			ImGui::BeginDisabled(tileCollision.type <= TileSolid);
			ImGui::InputFloat("Slope", &tileCollision.slope, 0.125f, 0.0625f);
			ImGui::InputFloat("Slope height", &tileCollision.slopeHeight, 0.125f, 0.0625f);
			tileCollision.slopeHeight = max(min(tileCollision.slopeHeight, 1.0f), 0.0f);
			ImGui::EndDisabled();

			// Draw slope visualisation
			if (tileCollision.type != TileEmpty) {
				ImVec2 rectStart = ImVec2(tilePos.x, tilePos.y);
				ImVec2 rectEnd = ImVec2(tilePos.x + tilePreviewSize, tilePos.y + tilePreviewSize);
				drawList->PushClipRect(rectStart, rectEnd);

				if (tileCollision.type == TileSolid) {
					drawList->AddRectFilled(rectStart, rectEnd, IM_COL32(0, 255, 0, 80));
				}
				else {
					ImVec2 points[4] = { 
						ImVec2(tilePos.x, tilePos.y + tilePreviewSize), 
						ImVec2(tilePos.x, tilePos.y + tilePreviewSize * (1 - tileCollision.slopeHeight)),
						ImVec2(tilePos.x + tilePreviewSize, tilePos.y + (1 - tileCollision.slope - tileCollision.slopeHeight) * tilePreviewSize),
						rectEnd 
					};
					ImVec2 pointsInverted[4] = {
						rectStart,
						ImVec2(tilePos.x + tilePreviewSize, tilePos.y),
						ImVec2(tilePos.x + tilePreviewSize, tilePos.y + (tileCollision.slopeHeight + tileCollision.slope) * tilePreviewSize),
						ImVec2(tilePos.x, tilePos.y + tilePreviewSize * tileCollision.slopeHeight)
					};
					drawList->AddConvexPolyFilled(tileCollision.type == TilePassThroughFlip ? pointsInverted : points, 4, IM_COL32(0, 255, 0, 80));
				}

				drawList->PopClipRect();
			}

			ImGui::End();
		}

	}
}