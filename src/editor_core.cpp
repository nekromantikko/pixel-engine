#include "editor_core.h"

namespace Editor {
	EditorContext* CreateEditorContext(Rendering::RenderContext* pRenderContext) {
		EditorContext* pContext = (EditorContext*)calloc(1, sizeof(EditorContext));

		pContext->chrTexture = Rendering::SetupEditorChrRendering(pRenderContext);
		pContext->paletteTexture = Rendering::SetupEditorPaletteRendering(pRenderContext);
		pContext->gameViewTexture = Rendering::SetupEditorGameViewRendering(pRenderContext);

		pContext->levelSelectionSize = { 1, 1 };
		pContext->levelSelectionOffset = { 0, 0 };

		return pContext;
	}

	void FreeEditorContext(EditorContext* pContext) {
		free(pContext);
	}
}