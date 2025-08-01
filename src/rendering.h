#pragma once
#include "typedef.h"
#include <SDL.h>
#include <limits>
#include <climits>
#include <cassert>

constexpr u32 MAX_SPRITE_COUNT = 4096;
constexpr u32 MAX_SPRITES_PER_SCANLINE = 64;

constexpr u32 BPP = 3;
constexpr u32 TILE_DIM_PIXELS = 8;
constexpr u32 TILE_BYTES = (TILE_DIM_PIXELS * TILE_DIM_PIXELS * BPP) / CHAR_BIT;

constexpr u32 METATILE_DIM_TILES = 2;
constexpr u32 METATILE_TILE_COUNT = METATILE_DIM_TILES * METATILE_DIM_TILES;
constexpr u32 METATILE_DIM_PIXELS = TILE_DIM_PIXELS * METATILE_DIM_TILES;

constexpr u32 CHR_DIM_TILES = 16;
constexpr u32 CHR_DIM_PIXELS = CHR_DIM_TILES * TILE_DIM_PIXELS;

constexpr u32 CHR_SIZE_TILES = CHR_DIM_TILES * CHR_DIM_TILES;
constexpr u32 CHR_SIZE_BYTES = CHR_SIZE_TILES * TILE_BYTES;
constexpr u32 CHR_PAGE_COUNT = 4;
constexpr u32 CHR_COUNT = CHR_PAGE_COUNT * 2;
constexpr u32 CHR_MEMORY_SIZE = CHR_SIZE_BYTES * CHR_COUNT;

constexpr u32 NAMETABLE_COUNT = 2;
constexpr u32 NAMETABLE_DIM_TILES = 64;
constexpr u32 NAMETABLE_DIM_TILES_LOG2 = 6;
constexpr u32 NAMETABLE_SIZE_TILES = NAMETABLE_DIM_TILES * NAMETABLE_DIM_TILES;
constexpr u32 NAMETABLE_DIM_METATILES = NAMETABLE_DIM_TILES >> 1;
constexpr u32 NAMETABLE_SIZE_METATILES = NAMETABLE_SIZE_TILES >> 2;

constexpr u32 NAMETABLE_DIM_PIXELS = NAMETABLE_DIM_TILES * TILE_DIM_PIXELS;

constexpr u32 VIEWPORT_WIDTH_TILES = NAMETABLE_DIM_TILES;
constexpr u32 VIEWPORT_HEIGHT_TILES = 36;
constexpr u32 VIEWPORT_SIZE_TILES = VIEWPORT_WIDTH_TILES * VIEWPORT_HEIGHT_TILES;
constexpr u32 VIEWPORT_WIDTH_PIXELS = VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS;
constexpr u32 VIEWPORT_HEIGHT_PIXELS = VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS;
constexpr u32 VIEWPORT_WIDTH_METATILES = VIEWPORT_WIDTH_TILES >> 1;
constexpr u32 VIEWPORT_HEIGHT_METATILES = VIEWPORT_HEIGHT_TILES >> 1;
constexpr u32 VIEWPORT_SIZE_METATILES = VIEWPORT_WIDTH_METATILES * VIEWPORT_HEIGHT_METATILES;

constexpr u32 COLOR_COUNT = 0x80;

constexpr u32 PALETTE_COUNT = 16;
constexpr u32 BG_PALETTE_COUNT = PALETTE_COUNT / 2;
constexpr u32 FG_PALETTE_COUNT = PALETTE_COUNT / 2;
constexpr u32 PALETTE_COLOR_COUNT = 8;
constexpr u32 PALETTE_MEMORY_SIZE = PALETTE_COUNT * PALETTE_COLOR_COUNT;

constexpr u32 SCANLINE_COUNT = VIEWPORT_HEIGHT_PIXELS;

struct alignas(4) Sprite {
	// y is first so we can easily set it offscreen when clearing
	s16 y;
	s16 x;
	u16 tileId : 10;
	u16 palette : 3;
	u16 priority : 1;
	u16 flipHorizontal : 1;
	u16 flipVertical : 1;
};

struct Scanline {
	s32 scrollX;
	s32 scrollY;
};

struct ChrTile {
	u64 p0;
	u64 p1;
	u64 p2;
};

struct ChrSheet {
	ChrTile tiles[0x100];
};

struct BgTile {
	u16 tileId : 10;
	u16 palette : 3;
	u16 unused : 1;
	u16 flipHorizontal : 1;
	u16 flipVertical : 1;
};

struct Nametable {
	BgTile tiles[NAMETABLE_SIZE_TILES];
};

struct Palette {
	u8 colors[PALETTE_COLOR_COUNT];
};

struct Metatile {
	BgTile tiles[METATILE_TILE_COUNT];
};

struct RenderSettings {
	bool useCRTFilter;
};
static constexpr RenderSettings DEFAULT_RENDER_SETTINGS = {
	true
};

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
	void CreateContext();
	void CreateSurface(SDL_Window* sdlWindow);
	void Init();
	void Free();
	void DestroyContext();

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

	EditorRenderBuffer* CreateEditorBuffer(u32 size, const void* data = nullptr);
	bool UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data);
	void FreeEditorBuffer(EditorRenderBuffer* pBuffer);

	EditorRenderTexture* CreateEditorTexture(u32 width, u32 height, u32 usage, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr);
	bool UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr);
	void* GetEditorTextureData(const EditorRenderTexture* pTexture);
	void FreeEditorTexture(EditorRenderTexture* pTexture);

	// Software
	void RenderEditorTexture(const EditorRenderTexture* pTexture);

	// Render pass
	void RenderEditor();
#endif
}