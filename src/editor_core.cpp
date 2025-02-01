#include "editor_core.h"

namespace Editor {
	EditorContext* CreateEditorContext() {
		EditorContext* pContext = (EditorContext*)calloc(1, sizeof(EditorContext));

		pContext->chrTextures = (ImTextureID*)calloc(PALETTE_COUNT, sizeof(ImTextureID));
		Rendering::CreateImGuiChrTextures(pContext->chrTextures);
		Rendering::CreateImGuiPaletteTexture(&pContext->paletteTexture);
		Rendering::CreateImGuiGameTexture(&pContext->gameViewTexture);

		pContext->levelSelectionSize = { 1, 1 };
		pContext->levelSelectionOffset = { 0, 0 };

		return pContext;
	}

	void FreeEditorContext(EditorContext* pContext) {
		Rendering::FreeImGuiChrTextures(pContext->chrTextures);
		Rendering::FreeImGuiPaletteTexture(&pContext->paletteTexture);
		Rendering::FreeImGuiGameTexture(&pContext->gameViewTexture);

		free(pContext->chrTextures);
		free(pContext);
	}
}