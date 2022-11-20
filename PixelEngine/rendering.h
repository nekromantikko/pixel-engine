#pragma once
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif
#include "typedef.h"

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

	struct CompoundSprite {
		Sprite *sprites;
		u32 spriteCount;
	};

	struct AnimatedSprite {
		CompoundSprite* frames;
		u32 frameCount;
		u32 frameLength;
	};

	struct RenderState {
		s32 scrollX;
		s32 scrollY;
		u32 bgChrIndex;
		u32 fgChrIndex;
	};

#define CHR_SHEET_SIZE 0x4000

#define TILE_SIZE 8

#define NAMETABLE_COUNT 2
#define NAMETABLE_SIZE 0x1000
#define NAMETABLE_ATTRIBUTE_OFFSET 0xF00
#define NAMETABLE_WIDTH_TILES 64
#define NAMETABLE_HEIGHT_TILES 60

#define VIEWPORT_WIDTH_TILES NAMETABLE_WIDTH_TILES
#define VIEWPORT_HEIGHT_TILES 36

#define SCANLINE_COUNT 288

	struct RenderContext;

	RenderContext* CreateRenderContext(Surface surface);
	void FreeRenderContext(RenderContext* pRenderContext);

	// This is temp stuff.....
	void BeginDraw(RenderContext* pRenderContext);
	void ExecuteHardcodedCommands(RenderContext* pRenderContext);
	void EndDraw(RenderContext* pRenderContext);

	void SetCurrentTime(RenderContext* pRenderContext, float seconds);

	// NES stuff
	void GetPaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* outColors);
	void SetPaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* colors);
	void ClearSprites(RenderContext* pContext, u32 offset, u32 count);
	void GetSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* outSprites);
	void SetSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* sprites);
	void UpdateNametable(RenderContext* pContext, u16 index, u16 count, u16 offset, u8* tiles);

	//NES commands
	void SetRenderState(RenderContext* pContext, u32 scanlineOffset, u32 scanlineCount, RenderState state);

	void ResizeSurface(RenderContext* pRenderContext, u32 width, u32 height);
}