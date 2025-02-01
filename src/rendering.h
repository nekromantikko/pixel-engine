#pragma once
#include "typedef.h"
#include <SDL.h>
#include <limits>
#include <cassert>

#ifdef EDITOR
#include <imgui.h>
#endif

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
constexpr u32 CHR_COUNT = 2;
constexpr u32 CHR_MEMORY_SIZE = CHR_SIZE_BYTES * CHR_COUNT;

constexpr u32 NAMETABLE_COUNT = 2;
constexpr u32 NAMETABLE_WIDTH_TILES = 64;
constexpr u32 NAMETABLE_WIDTH_TILES_LOG2 = 6;
constexpr u32 NAMETABLE_HEIGHT_TILES = 60;
constexpr u32 NAMETABLE_SIZE_TILES = NAMETABLE_WIDTH_TILES * NAMETABLE_HEIGHT_TILES;
constexpr u32 NAMETABLE_WIDTH_METATILES = NAMETABLE_WIDTH_TILES >> 1;
constexpr u32 NAMETABLE_WIDTH_METATILES_LOG2 = NAMETABLE_WIDTH_TILES_LOG2 - 1;
constexpr u32 NAMETABLE_HEIGHT_METATILES = NAMETABLE_HEIGHT_TILES >> 1;
constexpr u32 NAMETABLE_SIZE_METATILES = NAMETABLE_SIZE_TILES >> 2;

constexpr u32 NAMETABLE_WIDTH_PIXELS = NAMETABLE_WIDTH_TILES * TILE_DIM_PIXELS;
constexpr u32 NAMETABLE_HEIGHT_PIXELS = NAMETABLE_HEIGHT_TILES * TILE_DIM_PIXELS;

constexpr u32 NAMETABLE_WIDTH_ATTRIBUTES = NAMETABLE_WIDTH_TILES >> 2;
constexpr u32 NAMETABLE_HEIGHT_ATTRIBUTES = NAMETABLE_HEIGHT_TILES >> 2;
constexpr u32 NAMETABLE_ATTRIBUTE_COUNT = NAMETABLE_SIZE_TILES >> 4;

constexpr u32 NAMETABLE_MEMORY_SIZE = NAMETABLE_SIZE_TILES + NAMETABLE_ATTRIBUTE_COUNT;

constexpr u32 VIEWPORT_WIDTH_TILES = NAMETABLE_WIDTH_TILES;
constexpr u32 VIEWPORT_HEIGHT_TILES = 36;
constexpr u32 VIEWPORT_SIZE_TILES = VIEWPORT_WIDTH_TILES * VIEWPORT_HEIGHT_TILES;
constexpr u32 VIEWPORT_WIDTH_PIXELS = VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS;
constexpr u32 VIEWPORT_HEIGHT_PIXELS = VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS;
constexpr u32 VIEWPORT_WIDTH_METATILES = VIEWPORT_WIDTH_TILES >> 1;
constexpr u32 VIEWPORT_HEIGHT_METATILES = VIEWPORT_HEIGHT_TILES >> 1;
constexpr u32 VIEWPORT_SIZE_METATILES = VIEWPORT_WIDTH_METATILES * VIEWPORT_HEIGHT_METATILES;

constexpr u32 COLOR_COUNT = 0x80;

constexpr u32 PALETTE_COUNT = 8;
constexpr u32 PALETTE_COLOR_COUNT = 8;
constexpr u32 PALETTE_MEMORY_SIZE = PALETTE_COUNT * PALETTE_COLOR_COUNT;

constexpr u32 SCANLINE_COUNT = VIEWPORT_HEIGHT_PIXELS;

static_assert(NAMETABLE_WIDTH_TILES == (1 << NAMETABLE_WIDTH_TILES_LOG2));

struct alignas(4) Sprite {
	// y is first so we can easily set it offscreen when clearing
	u16 y : 9;
	u16 x : 9;
	u8 tileId;
	union {
		struct alignas(1) {
			u8 palette : 2;
			u8 unused : 3;
			u8 priority : 1;
			bool flipHorizontal : 1;
			bool flipVertical : 1;
		};
		u8 attributes;
	};
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

struct Nametable {
	u8 tiles[NAMETABLE_SIZE_TILES];
	u8 attributes[NAMETABLE_ATTRIBUTE_COUNT];
};

struct Palette {
	u8 colors[PALETTE_COLOR_COUNT];
};

struct Metatile {
	u8 tiles[METATILE_TILE_COUNT];
};

struct RenderSettings {
	bool useCRTFilter;
};
static constexpr RenderSettings DEFAULT_RENDER_SETTINGS = {
	true
};

namespace Rendering
{
	void CreateContext();
	void CreateSurface(SDL_Window* sdlWindow);
	void Init();
	void Free();
	void DestroyContext();

	// Generic commands
	void Render();
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
	void InitImGui(SDL_Window* sdlWindow);
	void BeginImGuiFrame();
	void ShutdownImGui();

	void CreateImGuiChrTextures(u32 index, ImTextureID* pTextures);
	void FreeImGuiChrTextures(u32 index, ImTextureID* pTextures);
	void CreateImGuiPaletteTexture(ImTextureID* pTexture);
	void FreeImGuiPaletteTexture(ImTextureID* pTexture);
	void CreateImGuiGameTexture(ImTextureID* pTexture);
	void FreeImGuiGameTexture(ImTextureID* pTexture);
#endif
}