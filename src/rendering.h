#pragma once
#include "typedef.h"
#include <SDL.h>
#include <limits>

#ifdef EDITOR
#include <imgui.h>
#endif

constexpr u32 MAX_SPRITE_COUNT = 4096;
constexpr u32 MAX_SPRITES_PER_SCANLINE = 64;

constexpr u32 BPP = 3;
constexpr u32 TILE_DIM_PIXELS = 8;
constexpr u32 TILE_BYTES = (TILE_DIM_PIXELS * TILE_DIM_PIXELS * BPP) / CHAR_BIT;

constexpr u32 CHR_DIM_TILES = 16;
constexpr u32 CHR_DIM_PIXELS = CHR_DIM_TILES * TILE_DIM_PIXELS;

constexpr u32 CHR_SIZE_TILES = CHR_DIM_TILES * CHR_DIM_TILES;
constexpr u32 CHR_SIZE_BYTES = CHR_SIZE_TILES * TILE_BYTES;
constexpr u32 CHR_COUNT = 2;
constexpr u32 CHR_MEMORY_SIZE = CHR_SIZE_BYTES * CHR_COUNT;

constexpr u32 NAMETABLE_COUNT = 2;
constexpr u32 NAMETABLE_SIZE = 0x1000;
constexpr u32 NAMETABLE_ATTRIBUTE_OFFSET = 0xF00;
constexpr u32 NAMETABLE_ATTRIBUTE_SIZE = 0x100;
constexpr u32 NAMETABLE_WIDTH_TILES = 64;
constexpr u32 NAMETABLE_HEIGHT_TILES = 60;

constexpr u32 VIEWPORT_WIDTH_TILES = NAMETABLE_WIDTH_TILES;
constexpr u32 VIEWPORT_HEIGHT_TILES = 36;

constexpr u32 COLOR_COUNT = 0x80;

constexpr u32 PALETTE_COUNT = 8;
constexpr u32 PALETTE_COLOR_COUNT = 8;
constexpr u32 PALETTE_MEMORY_SIZE = PALETTE_COUNT * PALETTE_COLOR_COUNT;

constexpr u32 SCANLINE_COUNT = 288;

struct Sprite {
	// y is first so we can easily set it offscreen when clearing
	s32 y;
	s32 x;
	u32 tileId;
	u32 attributes;
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
	u8 tiles[NAMETABLE_ATTRIBUTE_OFFSET];
	u8 attributes[NAMETABLE_ATTRIBUTE_SIZE];
};

struct Palette {
	u8 colors[PALETTE_COLOR_COUNT];
};

struct RenderSettings {
	bool useCRTFilter;
};
static constexpr RenderSettings DEFAULT_RENDER_SETTINGS = {
	true
};

struct RenderContext;

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

	void CreateImGuiChrTextures(ImTextureID* pTextures);
	void FreeImGuiChrTextures(ImTextureID* pTextures);
	void CreateImGuiPaletteTexture(ImTextureID* pTexture);
	void FreeImGuiPaletteTexture(ImTextureID* pTexture);
	void CreateImGuiGameTexture(ImTextureID* pTexture);
	void FreeImGuiGameTexture(ImTextureID* pTexture);
#endif
}