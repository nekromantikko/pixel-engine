#pragma once
#include "typedef.h"
#include "rendering.h"
#include <imgui.h>

namespace Editor {
	struct EditorContext {
		u8 paletteIndex = 0;
		ImTextureID* chrTexture;
		ImTextureID paletteTexture;
	};

	EditorContext* CreateEditorContext(Rendering::RenderContext* pRenderContext);
	void FreeEditorContext(EditorContext* pContext);
}