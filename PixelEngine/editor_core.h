#pragma once
#include "typedef.h"
#include "rendering.h"
#include <imgui.h>

namespace Editor {
	struct EditorContext {
		u8 chrPalette0Index = 0;
		u8 chrPalette1Index = 0;
		u8 chr0Selection = 0;
		u8 chr1Selection = 0;

		ImTextureID* chrTexture;
		ImTextureID paletteTexture;
	};

	EditorContext* CreateEditorContext(Rendering::RenderContext* pRenderContext);
	void FreeEditorContext(EditorContext* pContext);
}