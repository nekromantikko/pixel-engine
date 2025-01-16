#pragma once
#include "rendering.h"
#include "vector.h"

namespace Rendering
{
	namespace Util
	{
		void CreateChrSheet(const char* fname, ChrSheet* outSheet);
		void LoadPaletteColorsFromFile(const char* fname, u8* outColors);
		void GeneratePaletteColors(u32* data);
		void SavePaletteToFile(const char* fname);
		u8 GetPaletteIndexFromNametableTileAttrib(const Nametable& nametable, s32 xTile, s32 yTile);
		void CopyMetasprite(const Sprite* src, Sprite* dst, u32 count, IVec2 pos, bool hFlip, bool vFlip);
		void FlipSpritesHorizontal(Sprite* spr, u32 count);
		void FlipSpritesVertical(Sprite* spr, u32 count);
		void SetSpritesPalette(Sprite* spr, u32 count, u8 palette);
		void ClearSprites(Sprite* spr, u32 count);
		void CopyChrTiles(const ChrTile* src, ChrTile* dst, u32 count);
	}
}