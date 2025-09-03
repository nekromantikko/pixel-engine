#pragma once
#include "typedef.h"
#include <SDL.h>

struct RenderSettings {
	bool useCRTFilter;
};
static constexpr RenderSettings DEFAULT_RENDER_SETTINGS = {
	true
};

#ifdef EDITOR
struct EditorRenderTexture;
#endif

namespace Rendering
{
	void Init(SDL_Window* sdlWindow);
	void Free();

	// Generic commands
	void BeginFrame();
	void BeginRenderPass();
	void EndFrame();
	void WaitForAllCommands();
	void ResizeSurface(u32 width, u32 height);

	// Data access
	RenderSettings* GetSettings();

	// Editor stuff
#ifdef EDITOR
	void InitEditor(SDL_Window* sdlWindow);
	void BeginEditorFrame();
	void ShutdownEditor();

	size_t GetEditorTextureSize();
	bool InitEditorTexture(EditorRenderTexture* pTexture, u32 width, u32 height, const char* debugName = nullptr);
	bool UpdateEditorTexture(const EditorRenderTexture* pTexture);
	u32* GetEditorTexturePixels(const EditorRenderTexture* pTexture);
	void* GetEditorTextureTexture(const EditorRenderTexture* pTexture);
	void FreeEditorTexture(EditorRenderTexture* pTexture);

	// Render pass
	void RenderEditor();
#endif
}