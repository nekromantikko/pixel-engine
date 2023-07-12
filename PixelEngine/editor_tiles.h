#pragma once
#include "editor_core.h"
#include "rendering.h"

namespace Editor {
	namespace Tiles {

		void DrawBgCollisionWindow(EditorContext* pContext);
		void DrawCollisionEditor(EditorContext* pContext, Rendering::RenderContext* pRenderContext);

	}
}