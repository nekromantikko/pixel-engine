#include "editor_util.h"
#include "editor_core.h"
#include <cmath>

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

		ImVec2 DrawTileGrid(EditorContext* pContext, r32 size, s32 divisions, bool* focused) {
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

			return topLeft;
		}

	}
}