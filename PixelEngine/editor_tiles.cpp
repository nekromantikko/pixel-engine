#include "editor_tiles.h"
#include "editor_util.h"
#include "collision.h"
#include <stdio.h>

namespace Editor {
	namespace Tiles {
		constexpr r32 tilePreviewSize = 64;
		constexpr r32 pixelSize = tilePreviewSize / TILE_SIZE;

		constexpr u32 bgCollisionTypeCount = 5;
		constexpr const char* bgCollisionTypeNames[bgCollisionTypeCount] = { "Empty", "Solid", "PassThrough", "JumpThrough", "PassThroughFlip" };

		void DrawBgCollisionWindow(EditorContext* pContext) {
			ImGui::Begin("Background Tiles");

			Collision::TileCollision* bgCollision = Collision::GetBgCollisionPtr();

			if (ImGui::Button("Save")) {
				Collision::SaveBgCollision("bg.til");
			}
			ImGui::SameLine();
			if (ImGui::Button("Revert changes")) {
				Collision::LoadBgCollision("bg.til");
			}

			ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_NoBordersInBody;
			ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;

			if (ImGui::BeginTable("Background tiles", 4, flags)) {
				ImGui::TableSetupColumn("Tile");
				ImGui::TableSetupColumn("Type");
				ImGui::TableSetupColumn("Slope");
				ImGui::TableSetupColumn("Slope height");
				ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
				ImGui::TableHeadersRow();

				for (u32 i = 0; i < 256; i++) {
					Collision::TileCollision tile = bgCollision[i];

					char labelStr[5];
					snprintf(labelStr, 5, "0x%02x", i);
					bool selected = pContext->chrSelection[0] == i;

					ImGui::PushID(i);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					if (ImGui::Selectable(labelStr, selected, selectableFlags, ImVec2(0, 0))) {
						pContext->chrSelection[0] = i;

						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(bgCollisionTypeNames[tile.type]);
					ImGui::TableNextColumn();
					ImGui::Text("%f", tile.slope);
					ImGui::TableNextColumn();
					ImGui::Text("%f", tile.slopeHeight);
					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::End();
		}

		void DrawCollisionEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext) {
			Collision::TileCollision* bgCollision = Collision::GetBgCollisionPtr();

			ImGui::Begin("Tile Editor");

			u8 selectedTileIndex = pContext->chrSelection[0];
			ImGui::Text("0x%02x", selectedTileIndex);

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 tilePos = Util::DrawTileGrid(pContext, tilePreviewSize, 1);
			ImVec2 tileCoord = Util::GetTileCoord(selectedTileIndex);
			ImVec2 tileStart = Util::TileCoordToTexCoord(tileCoord, 0);
			ImVec2 tileEnd = Util::TileCoordToTexCoord(ImVec2(tileCoord.x + 1, tileCoord.y + 1), 0);
			drawList->AddImage(pContext->chrTexture[pContext->chrPaletteIndex[0]], tilePos, ImVec2(tilePos.x + tilePreviewSize, tilePos.y + tilePreviewSize), tileStart, tileEnd);

			Collision::TileCollision& tileCollision = bgCollision[selectedTileIndex];
			ImGui::SliderInt("Type", (s32*)&tileCollision.type, 0, bgCollisionTypeCount-1, bgCollisionTypeNames[tileCollision.type]);

			ImGui::BeginDisabled(tileCollision.type <= Collision::TileSolid);
			ImGui::InputFloat("Slope", &tileCollision.slope, 0.125f, 0.0625f);
			ImGui::InputFloat("Slope height", &tileCollision.slopeHeight, 0.125f, 0.0625f);
			tileCollision.slopeHeight = max(min(tileCollision.slopeHeight, 1.0f), 0.0f);
			ImGui::EndDisabled();

			// Draw slope visualisation
			if (tileCollision.type != Collision::TileEmpty) {
				ImVec2 rectStart = ImVec2(tilePos.x, tilePos.y);
				ImVec2 rectEnd = ImVec2(tilePos.x + tilePreviewSize, tilePos.y + tilePreviewSize);
				drawList->PushClipRect(rectStart, rectEnd);

				if (tileCollision.type == Collision::TileSolid) {
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
					drawList->AddConvexPolyFilled(tileCollision.type == Collision::TilePassThroughFlip ? pointsInverted : points, 4, IM_COL32(0, 255, 0, 80));
				}

				drawList->PopClipRect();
			}

			ImGui::End();
		}

	}
}