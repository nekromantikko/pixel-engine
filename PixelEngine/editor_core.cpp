#include "editor_core.h"

namespace Editor {
	EditorContext* CreateEditorContext(Rendering::RenderContext* pRenderContext) {
		EditorContext* pContext = (EditorContext*)calloc(1, sizeof(EditorContext));

		pContext->chrTexture = Rendering::SetupDebugChrRendering(pRenderContext);
		pContext->paletteTexture = Rendering::SetupDebugPaletteRendering(pRenderContext);

		return pContext;
	}

	void FreeEditorContext(EditorContext* pContext) {
		free(pContext);
	}
}