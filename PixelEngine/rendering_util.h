#pragma once
#include "rendering.h"
#include "typedef.h"

namespace Rendering
{
	namespace Util
	{
		struct Tile {
			u8 pixels[64];
		};

		void CreateChrSheet(const char* fname, u8* outBytes);
		u8 GetPaletteIndexFromNametableTileAttrib(u8* pNametable, s32 xTile, s32 yTile);
		void WriteMetasprite(Rendering::RenderContext* pContext, Sprite* sprites, u32 count, u32 offset, s32 x, s32 y, bool flip);
	}
}