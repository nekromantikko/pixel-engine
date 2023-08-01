#pragma once
#include "rendering.h"
#include "vector.h"

namespace Rendering
{
	namespace Util
	{
		void CreateChrSheet(const char* fname, CHRSheet* outBytes);
		u8 GetPaletteIndexFromNametableTileAttrib(u8* pNametable, s32 xTile, s32 yTile);
		void WriteMetasprite(RenderContext* pContext, Sprite* sprites, u32 count, u32 offset, IVec2 pos, bool hFlip, bool vFlip);
		void WriteChrTiles(RenderContext* pContext, bool sheetIndex, u32 tileCount, u8 srcOffset, u8 dstOffset, CHRSheet* sheet);
	}
}