#pragma once
#include "editor_core.h"
#include "rendering.h"

namespace Editor {
	namespace Tiles {

		void DrawMetatileEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext);
		void DrawTilesetEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext);
	}
}