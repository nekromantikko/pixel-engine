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
		s32 GetNametableTileIndexFromTileOffset(const glm::ivec2& tileOffset);
		s32 GetNametableTileIndexFromMetatileOffset(const glm::ivec2& metatileOffset);
		s32 GetNametableIndexFromTilePos(const glm::ivec2& tilePos);
		glm::ivec2 GetNametableOffsetFromTilePos(const glm::ivec2& tilePos);
		s32 GetNametableIndexFromMetatilePos(const glm::ivec2& metatilePos);
		glm::ivec2 GetNametableOffsetFromMetatilePos(const glm::ivec2& metatilePos);
		void SetNametableTile(Nametable* pNametables, const glm::ivec2& tilePos, u8 tileIndex);
		void GetNametableMetatile(const Nametable* pNametable, u32 metatileIndex, Metatile& outMetatile, s32& outPalette);
		void SetNametableMetatile(Nametable* pNametable, const glm::ivec2& metatileOffset, const Metatile& metatile, const s32 palette);
		void ClearNametable(Nametable* pNametable, u8 tile = 0, s32 palette = 0);

		void CreateChrSheet(const char* fname, ChrSheet* outSheet);
		void LoadPaletteColorsFromFile(const char* fname, u8* outColors);
		void GeneratePaletteColors(u32* data);
		void SavePaletteToFile(const char* fname);
		s16 SignExtendSpritePos(u16 spritePos);
		
		void CopyChrTiles(const ChrTile* src, ChrTile* dst, u32 count);
	}
}