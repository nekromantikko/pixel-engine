#pragma once
#include "typedef.h"
#include "rendering.h"
#include <imgui.h>

namespace Editor {
	struct EditorContext {
		u32 chrPaletteIndex[2];
		u32 chrSelection[2];

		u8 levelClipboard[((VIEWPORT_WIDTH_TILES / 2) + 1) * ((VIEWPORT_HEIGHT_TILES / 2) + 1)];
		ImVec2 levelSelectionSize;
		ImVec2 levelSelectionOffset;

		ImTextureID* chrTexture;
		ImTextureID paletteTexture;
		ImTextureID gameViewTexture;
	};

	EditorContext* CreateEditorContext(Rendering::RenderContext* pRenderContext);
	void FreeEditorContext(EditorContext* pContext);
}