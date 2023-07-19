#pragma once
#include "rendering.h"
#include "typedef.h"

namespace Rendering
{
	namespace Util
	{
		void CreateChrSheet(const char* fname, CHRSheet* outBytes);
		u8 GetPaletteIndexFromNametableTileAttrib(u8* pNametable, s32 xTile, s32 yTile);
		void WriteMetasprite(RenderContext* pContext, Sprite* sprites, u32 count, u32 offset, s32 x, s32 y, bool flip);
		void WriteChrTiles(RenderContext* pContext, bool sheetIndex, u32 tileCount, u8 srcOffset, u8 dstOffset, CHRSheet* sheet);
	}
}