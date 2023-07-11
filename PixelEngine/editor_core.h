#pragma once
#include "typedef.h"
#include "rendering.h"
#include <imgui.h>

namespace Editor {
	struct EditorContext {
		u8 chrPaletteIndex[2] = { 0,0 };
		u8 chrSelection[2] = { 0,0 };

		ImTextureID* chrTexture;
		ImTextureID paletteTexture;
	};

	EditorContext* CreateEditorContext(Rendering::RenderContext* pRenderContext);
	void FreeEditorContext(EditorContext* pContext);
}