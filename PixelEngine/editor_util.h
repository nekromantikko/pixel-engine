#pragma once
#include "editor_core.h"

namespace Editor {
	namespace Util {
		
		ImVec2 GetTileCoord(u8 index);
		u8 GetTileIndex(ImVec2 tileCoord);
		ImVec2 TileCoordToTexCoord(ImVec2 coord, bool chrIndex);
		ImVec2 TexCoordToTileCoord(ImVec2 normalized);
		ImVec2 DrawTileGrid(EditorContext* pContext, r32 size, s32 divisions, u32* selection = nullptr, bool* focused = nullptr);
		void DrawTileGridSelection(EditorContext* pContext, ImVec2 gridPos, r32 gridSize, s32 divisions, u32 selection);

	}
}