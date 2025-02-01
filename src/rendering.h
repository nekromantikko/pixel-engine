#pragma once
#include "typedef.h"
#include <imgui.h>
#include <SDL.h>

constexpr u32 MAX_SPRITE_COUNT = 4096;
constexpr u32 MAX_SPRITES_PER_SCANLINE = 64;

constexpr u32 CHR_SHEET_SIZE = 0x1800;
constexpr u32 CHR_MEMORY_SIZE = CHR_SHEET_SIZE * 2;

constexpr u32 TILE_SIZE = 8;

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
constexpr u32 PALETTE_DATA_SIZE = PALETTE_COUNT * PALETTE_COLOR_COUNT;

constexpr u32 SCANLINE_COUNT = 288;

namespace Rendering
{
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
		ChrTile tiles[256];
	};

	struct Nametable {
		u8 tiles[NAMETABLE_ATTRIBUTE_OFFSET];
		u8 attributes[NAMETABLE_ATTRIBUTE_SIZE];
	};

	struct Palette {
		u8 colors[PALETTE_COLOR_COUNT];
	};

	struct Quad {
		r32 x, y, w, h;
	};

	const Quad defaultQuad = { 0, 0, 1, 1 };

	struct RenderContext;

	struct Settings {
		bool useCRTFilter;
	};

	const Settings defaultSettings = {
		true
	};

	RenderContext* CreateRenderContext(SDL_Window *sdlWindow);
	void WaitForAllCommands(RenderContext* pRenderContext);
	void FreeRenderContext(RenderContext* pRenderContext);
	Settings* GetSettingsPtr(RenderContext* pContext);

	// Generic commands
	void Render(RenderContext* pRenderContext);
	void ResizeSurface(RenderContext* pRenderContext, u32 width, u32 height);

	// NES commands
	Palette* GetPalettePtr(RenderContext* pContext, u32 paletteIndex);
	Sprite* GetSpritesPtr(RenderContext* pContext, u32 offset);
	ChrSheet* GetChrPtr(RenderContext* pContext, u32 sheetIndex);
	Nametable* GetNametablePtr(RenderContext* pContext, u32 index);
	Scanline* GetScanlinePtr(RenderContext* pContext, u32 offset);

	// ImGui
	void InitImGui(RenderContext* pContext, SDL_Window* sdlWindow);
	void BeginImGuiFrame(RenderContext* pContext);
	void ShutdownImGui();

	// DEBUG
	ImTextureID* SetupEditorChrRendering(RenderContext* pContext);
	ImTextureID SetupEditorPaletteRendering(RenderContext* pContext);
	ImTextureID SetupEditorGameViewRendering(RenderContext* pContext);
}