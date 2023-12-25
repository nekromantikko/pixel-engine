#include "editor_util.h"
#include "editor_core.h"
#include <cmath>
#include "tileset.h"

namespace Editor {
	namespace Util {

		ImVec2 GetTileCoord(u8 index) {
			ImVec2 coord = ImVec2(index % 16, index / 16);
			return coord;
		}

		u8 GetTileIndex(ImVec2 tileCoord) {
			u8 index = tileCoord.y * 16 + tileCoord.x;
			return index;
		}

		ImVec2 TileCoordToTexCoord(ImVec2 coord, bool chrIndex) {
			ImVec2 normalizedCoord = ImVec2(coord.x / 32.0f, coord.y / 16.0f);
			if (chrIndex) {
				normalizedCoord.x += 0.5;
			}
			return normalizedCoord;
		}

		ImVec2 TexCoordToTileCoord(ImVec2 normalized) {
			if (normalized.x >= 0.5) {
				normalized.x -= 0.5;
			}

			ImVec2 tileCoord = ImVec2(floor(normalized.x * 32), floor(normalized.y * 16));
			return tileCoord;
		}

		ImVec2 DrawTileGrid(EditorContext* pContext, r32 size, s32 divisions, u32* selection, bool* focused) {
			r32 gridStep = size / divisions;

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 topLeft = ImGui::GetCursorScreenPos();
			ImVec2 btmRight = ImVec2(topLeft.x + size, topLeft.y + size);

			// Invisible button to prevent dragging window
			ImGui::InvisibleButton("##canvas", ImVec2(size, size));

			if (focused != nullptr) {
				*focused = ImGui::IsItemActive();
			}

			drawList->AddImage(pContext->paletteTexture, topLeft, btmRight, ImVec2(0, 0), ImVec2(0.015625f, 1.0f));
			for (r32 x = 0; x < size; x += gridStep)
				drawList->AddLine(ImVec2(topLeft.x + x, topLeft.y), ImVec2(topLeft.x + x, btmRight.y), IM_COL32(200, 200, 200, 40));
			for (r32 y = 0; y < size; y += gridStep)
				drawList->AddLine(ImVec2(topLeft.x, topLeft.y + y), ImVec2(btmRight.x, topLeft.y + y), IM_COL32(200, 200, 200, 40));

			if (selection != nullptr && ImGui::IsItemActive()) {
				// Handle selection
				ImGuiIO& io = ImGui::GetIO();
				bool gridClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x >= topLeft.x && io.MousePos.x < topLeft.x + size && io.MousePos.y >= topLeft.y && io.MousePos.y < topLeft.y + size;
				if (gridClicked) {
					ImVec2 mousePosRelative = ImVec2(io.MousePos.x - topLeft.x, io.MousePos.y - topLeft.y);
					ImVec2 clickedTileCoord = ImVec2(floor(mousePosRelative.x / gridStep), floor(mousePosRelative.y / gridStep));
					u32 clickedTileIndex = clickedTileCoord.y * divisions + clickedTileCoord.x;
					*selection = clickedTileIndex;
				}
			}

			return topLeft;
		}

		void DrawTileGridSelection(EditorContext* pContext, ImVec2 gridPos, r32 gridSize, s32 divisions, u32 selection) {
			r32 gridStep = gridSize / divisions;

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 selectedTilePos = ImVec2(gridPos.x + gridStep * (selection % divisions), gridPos.y + gridStep * (selection / divisions));
			drawList->AddRect(selectedTilePos, ImVec2(selectedTilePos.x + gridStep, selectedTilePos.y + gridStep), IM_COL32(255, 255, 255, 255));
		}

		void DrawMetatile(EditorContext* pContext, u32 index, ImVec2 pos, r32 tileSize, ImU32 color) {
			Tileset::Metatile& metatile = Tileset::GetMetatile(index);

			s32 palette = Tileset::GetPalette(index);

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			for (s32 t = 0; t < 4; t++) {
				ImVec2 tileCoord = ImVec2(t % 2, t / 2);
				ImVec2 tileOffset = ImVec2(pos.x + tileCoord.x * tileSize, pos.y + tileCoord.y * tileSize);
				u8 tileIndex = metatile.tiles[t];
				ImVec2 tileChrCoord = GetTileCoord(tileIndex);
				ImVec2 tileStart = TileCoordToTexCoord(tileChrCoord, 0);
				ImVec2 tileEnd = TileCoordToTexCoord(ImVec2(tileChrCoord.x + 1, tileChrCoord.y + 1), 0);
				drawList->AddImage(pContext->chrTexture[palette], tileOffset, ImVec2(tileOffset.x + tileSize, tileOffset.y + tileSize), tileStart, tileEnd, color);
			}
		}

	}
}