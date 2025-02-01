#pragma once
#include "editor_core.h"
#include "rendering.h"

namespace Editor {
	namespace Debug {

		void DrawNametable(EditorContext* pContext, ImVec2 tablePos, u8* pNametable);
		void DrawDebugWindow(EditorContext* pContext);

	}
}