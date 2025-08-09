#pragma once
#include "typedef.h"
#include <SDL.h>

struct RenderSettings {
	bool useCRTFilter;
};
static constexpr RenderSettings DEFAULT_RENDER_SETTINGS = {
	true
};

struct Palette;
struct Sprite;
struct ChrSheet;
struct Nametable;
struct Scanline;

#ifdef EDITOR
enum EditorTextureUsage {
	EDITOR_TEXTURE_USAGE_CHR,
	EDITOR_TEXTURE_USAGE_PALETTE,
	EDITOR_TEXTURE_USAGE_COLOR
};

struct EditorRenderTexture;
struct EditorRenderBuffer;
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
	RenderSettings* GetSettingsPtr();
	Palette* GetPalettePtr(u32 paletteIndex);
	Sprite* GetSpritesPtr(u32 offset);
	ChrSheet* GetChrPtr(u32 sheetIndex);
	Nametable* GetNametablePtr(u32 index);
	Scanline* GetScanlinePtr(u32 offset);

	// Editor stuff
#ifdef EDITOR
	void InitEditor(SDL_Window* sdlWindow);
	void BeginEditorFrame();
	void ShutdownEditor();

	size_t GetEditorBufferSize();
	bool InitEditorBuffer(EditorRenderBuffer* pBuffer, size_t size, const void* data = nullptr);
	bool UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data);
	void FreeEditorBuffer(EditorRenderBuffer* pBuffer);

	size_t GetEditorTextureSize();
	bool InitEditorTexture(EditorRenderTexture* pTexture, u32 width, u32 height, u32 usage, u32 chrSheetOffset = 0, u32 chrPaletteOffset = 0, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr);
	bool UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr);
	void* GetEditorTextureData(const EditorRenderTexture* pTexture);
	void FreeEditorTexture(EditorRenderTexture* pTexture);

	// Software
	void RenderEditorTexture(const EditorRenderTexture* pTexture);

	// Render pass
	void RenderEditor();
#endif
}