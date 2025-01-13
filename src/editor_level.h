#pragma once
#include "editor_core.h"
#include "rendering.h"

namespace Editor {
	namespace LevelEditor {

		void DrawGameWindow(EditorContext* pContext, Rendering::RenderContext* pRenderContext);
		void DrawActorList();
		void DrawLevelList(Rendering::RenderContext* pRenderContext);
	}
}