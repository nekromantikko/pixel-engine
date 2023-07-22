#include "editor_tiles.h"
#include "editor_util.h"
#include "collision.h"
#include <stdio.h>

namespace Editor {
	namespace Tiles {
		constexpr r32 tilePreviewSize = 64;
		constexpr r32 pixelSize = tilePreviewSize / TILE_SIZE;

		constexpr u32 bgCollisionTypeCount = 5;
		constexpr const char* bgCollisionTypeNames[bgCollisionTypeCount] = { "Empty", "Solid", "Slope", "JumpThrough", "SlopeFlip" };

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
				ImGui::TableSetupColumn("Slope start");
				ImGui::TableSetupColumn("Slope end");
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
					ImGui::Text("(%f, %f)", tile.slopeStart.x, tile.slopeStart.y);
					ImGui::TableNextColumn();
					ImGui::Text("(%f, %f)", tile.slopeEnd.x, tile.slopeEnd.y);
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
			r32* slopeStart = (r32*)&tileCollision.slopeStart;
			r32* slopeEnd = (r32*)&tileCollision.slopeEnd;
			r32 tolerance = 0.00001f;
			if (ImGui::InputFloat2("Slope start", slopeStart)) {
				tileCollision.slopeStart.x = max(min(tileCollision.slopeStart.x, tileCollision.slopeEnd.x - tolerance), 0.0f);
				tileCollision.slopeStart.y = max(min(tileCollision.slopeStart.y, 1.0f), 0.0f);
			}
			if (ImGui::InputFloat2("Slope end", slopeEnd)) {
				tileCollision.slopeEnd.x = min(max(tileCollision.slopeStart.x + tolerance, tileCollision.slopeEnd.x), 1.0f);
				tileCollision.slopeEnd.y = min(max(tileCollision.slopeEnd.y, 0.0f), 1.0f);
			}

			ImGui::TextUnformatted("Slope presets");
			if (ImGui::Button("0")) {
				tileCollision.slopeStart = Vec2{ 0.0f, 0.0f };
				tileCollision.slopeEnd = Vec2{ 1.0f, 0.0f };
			}
			ImGui::SameLine();
			if (ImGui::Button("1")) {
				tileCollision.slopeStart = Vec2{ 0.0f, 1.0f };
				tileCollision.slopeEnd = Vec2{ 1.0f, 0.0f };
			}
			ImGui::SameLine();
			if (ImGui::Button("-1")) {
				tileCollision.slopeStart = Vec2{ 0.0f, 0.0f };
				tileCollision.slopeEnd = Vec2{ 1.0f, 1.0f };
			}
			ImGui::SameLine();
			if (ImGui::Button("2")) {
				tileCollision.slopeStart = Vec2{ 0.0f, 1.0f };
				tileCollision.slopeEnd = Vec2{ 1.0f, 0.5f };
			}
			ImGui::SameLine();
			if (ImGui::Button("-2")) {
				tileCollision.slopeStart = Vec2{ 0.0f, 0.5f };
				tileCollision.slopeEnd = Vec2{ 1.0f, 1.0f };
			}
			ImGui::EndDisabled();

			ImGui::TextUnformatted("Slope height");
			if (ImGui::Button("+")) {
				tileCollision.slopeStart.y += 0.125f;
				tileCollision.slopeEnd.y += 0.125f;
			}
			ImGui::SameLine();
			if (ImGui::Button("-")) {
				tileCollision.slopeStart.y -= 0.125f;
				tileCollision.slopeEnd.y -= 0.125f;
			}

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
						ImVec2(tilePos.x, tilePos.y + Collision::GetTileSurfaceY(tileCollision, 0.0f) * tilePreviewSize),
						ImVec2(tilePos.x + tilePreviewSize, tilePos.y + Collision::GetTileSurfaceY(tileCollision, 1.0f) * tilePreviewSize),
						rectEnd,
						ImVec2(tilePos.x, tilePos.y + tilePreviewSize)
					};
					ImVec2 pointsInverted[4] = {
						rectStart,
						ImVec2(tilePos.x + tilePreviewSize, tilePos.y),
						ImVec2(tilePos.x + tilePreviewSize, tilePos.y + Collision::GetTileSurfaceY(tileCollision, 1.0f) * tilePreviewSize),
						ImVec2(tilePos.x, tilePos.y + Collision::GetTileSurfaceY(tileCollision, 0.0f) * tilePreviewSize),
					};
					drawList->AddConvexPolyFilled(tileCollision.type == Collision::TileSlopeFlip ? pointsInverted : points, 4, IM_COL32(0, 255, 0, 80));
				}

				drawList->PopClipRect();
			}

			ImGui::End();
		}

	}
}