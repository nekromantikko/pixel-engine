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

	struct CHRSheet {
		u64 p0[256];
		u64 p1[256];
		u64 p2[256];
	};

#define TILE_SIZE 8

#define NAMETABLE_COUNT 2
#define NAMETABLE_SIZE 0x1000
#define NAMETABLE_ATTRIBUTE_OFFSET 0xF00
#define NAMETABLE_ATTRIBUTE_SIZE 0x100
#define NAMETABLE_WIDTH_TILES 64
#define NAMETABLE_HEIGHT_TILES 60

#define VIEWPORT_WIDTH_TILES NAMETABLE_WIDTH_TILES
#define VIEWPORT_HEIGHT_TILES 36

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
	void FreeRenderContext(RenderContext* pRenderContext);
	Settings* GetSettingsPtr(RenderContext* pContext);

	// Generic commands
	void Render(RenderContext* pRenderContext);
	void SetCurrentTime(RenderContext* pRenderContext, float seconds);
	void ResizeSurface(RenderContext* pRenderContext, u32 width, u32 height);

	// NES commands
	void ReadPaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* outColors);
	void WritePaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* colors);
	void ClearSprites(RenderContext* pContext, u32 offset, u32 count);
	void WriteSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* sprites);
	void ReadSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* outSprites);
	void WriteChrMemory(RenderContext* pContext, u32 size, u32 offset, u8* bytes);
	void ReadChrMemory(RenderContext* pContext, u32 size, u32 offset, u8* outBytes);
	void WriteNametable(RenderContext* pContext, u16 index, u16 count, u16 offset, u8* tiles);
	void ReadNametable(RenderContext* pContext, u16 index, u16 count, u16 offset, u8* outTiles);
	void SetRenderState(RenderContext* pContext, u32 scanlineOffset, u32 scanlineCount, RenderState state);

	// ImGui
	void InitImGui(RenderContext* pContext);
	void BeginImGuiFrame(RenderContext* pContext);
	void ShutdownImGui();

	// DEBUG
	ImTextureID* SetupDebugChrRendering(RenderContext* pContext);
	ImTextureID SetupDebugPaletteRendering(RenderContext* pContext);
}