#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "rendering.h"

namespace Rendering
{
	namespace Util
	{
		// Nametable utils
		s32 GetPalette(u8 attribute, u8 offset);
		u8 SetPalette(u8 attribute, u8 offset, s32 palette);
		void GetNametableMetatile(const Nametable* pNametable, u32 metatileIndex, Metatile& outMetatile, s32& outPalette);
		void SetNametableMetatile(Nametable* pNametable, u32 x, u32 y, const Metatile& metatile, const s32 palette);
		void ClearNametable(Nametable* pNametable, u8 tile = 0, s32 palette = 0);

		void CreateChrSheet(const char* fname, ChrSheet* outSheet);
		void LoadPaletteColorsFromFile(const char* fname, u8* outColors);
		void GeneratePaletteColors(u32* data);
		void SavePaletteToFile(const char* fname);
		void CopyMetasprite(const Sprite* src, Sprite* dst, u32 count, glm::ivec2 pos, bool hFlip, bool vFlip, s32 paletteOverride = -1);
		void ClearSprites(Sprite* spr, u32 count);
		void CopyChrTiles(const ChrTile* src, ChrTile* dst, u32 count);
	}
}