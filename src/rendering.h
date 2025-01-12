#pragma once
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif
#include "typedef.h"
#include <imgui.h>

namespace Rendering
{
#ifdef PLATFORM_WINDOWS
	struct WindowsSurface {
		HINSTANCE hInstance;
		HWND hWnd;
	};
	#define Surface WindowsSurface
#endif

#define MAX_SPRITE_COUNT 4096
#define MAX_SPRITES_PER_SCANLINE 64
	struct Sprite {
		// y is first so we can easily set it offscreen when clearing
		s32 y;
		s32 x;
		u32 tileId;
		u32 attributes;
	};

	struct RenderState {
		s32 scrollX;
		s32 scrollY;
	};

#define CHR_SHEET_SIZE 0x1800
#define CHR_MEMORY_SIZE CHR_SHEET_SIZE * 2
	struct ChrTile {
		u64 p0;
		u64 p1;
		u64 p2;
	};

	struct ChrSheet {
		ChrTile tiles[256];
	};

#define TILE_SIZE 8

#define NAMETABLE_COUNT 2
#define NAMETABLE_SIZE 0x1000
#define NAMETABLE_ATTRIBUTE_OFFSET 0xF00
#define NAMETABLE_ATTRIBUTE_SIZE 0x100
#define NAMETABLE_WIDTH_TILES 64
#define NAMETABLE_HEIGHT_TILES 60
	struct Nametable {
		u8 tiles[NAMETABLE_ATTRIBUTE_OFFSET];
		u8 attributes[NAMETABLE_ATTRIBUTE_SIZE];
	};

#define VIEWPORT_WIDTH_TILES NAMETABLE_WIDTH_TILES
#define VIEWPORT_HEIGHT_TILES 36

	struct Palette {
		u8 colors[8];
	};

#define SCANLINE_COUNT 288

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

	RenderContext* CreateRenderContext(Surface surface);
	void WaitForAllCommands(RenderContext* pRenderContext);
	void FreeRenderContext(RenderContext* pRenderContext);
	Settings* GetSettingsPtr(RenderContext* pContext);

	// Generic commands
	void Render(RenderContext* pRenderContext);
	void ResizeSurface(RenderContext* pRenderContext, u32 width, u32 height);

	// NES commands
	Palette* GetPalettePtr(RenderContext* pContext, u8 paletteIndex);
	Sprite* GetSpritesPtr(RenderContext* pContext, u32 offset);
	ChrSheet* GetChrPtr(RenderContext* pContext, u16 sheetIndex);
	Nametable* GetNametablePtr(RenderContext* pContext, u16 index);
	void SetRenderState(RenderContext* pContext, u32 scanlineOffset, u32 scanlineCount, RenderState state);

	// ImGui
	void InitImGui(RenderContext* pContext);
	void BeginImGuiFrame(RenderContext* pContext);
	void ShutdownImGui();

	// DEBUG
	ImTextureID* SetupEditorChrRendering(RenderContext* pContext);
	ImTextureID SetupEditorPaletteRendering(RenderContext* pContext);
	ImTextureID SetupEditorGameViewRendering(RenderContext* pContext);
}