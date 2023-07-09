#pragma once
#include "editor_core.h"

namespace Editor {
	namespace Util {
		
		ImVec2 GetTileCoord(u8 index);
		ImVec2 TileCoordToTexCoord(ImVec2 coord, bool chrIndex);
		ImVec2 DrawTileGrid(EditorContext* pContext, r32 size, s32 divisions);

	}
}