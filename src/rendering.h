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
	EDITOR_TEXTURE_USAGE_CHR_INDEXED,
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

	EditorRenderBuffer* CreateEditorBuffer(size_t size, const void* data = nullptr);
	bool UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data);
	void FreeEditorBuffer(EditorRenderBuffer* pBuffer);

	EditorRenderTexture* CreateEditorTexture(u32 width, u32 height, u32 usage, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr);
	bool UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr);
	void* GetEditorTextureData(const EditorRenderTexture* pTexture);
	void FreeEditorTexture(EditorRenderTexture* pTexture);

	// Software
	void RenderEditorTexture(const EditorRenderTexture* pTexture);
	void RenderEditorTextureIndexed(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer);

	// ImGui callback for palette application
	void ImGuiPaletteCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd);
	
	// Helper functions for indexed CHR system
	EditorRenderTexture* CreateIndexedChrTexture(u32 chrIndex);
	void AddIndexedChrImageWithPalette(ImDrawList* drawList, u32 chrIndex, u8 palette, const ImVec2& pos, const ImVec2& size, const ImVec2& uvMin = ImVec2(0, 0), const ImVec2& uvMax = ImVec2(1, 1));

	// Render pass
	void RenderEditor();
#endif
}