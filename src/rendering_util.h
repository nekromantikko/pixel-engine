#pragma once
#include "core_types.h"

namespace Rendering
{
	namespace Util
	{
		s32 GetNametableTileIndexFromTileOffset(const glm::ivec2& tileOffset);
		s32 GetNametableTileIndexFromMetatileOffset(const glm::ivec2& metatileOffset);
		s32 GetNametableIndexFromTilePos(const glm::ivec2& tilePos);
		glm::ivec2 GetNametableOffsetFromTilePos(const glm::ivec2& tilePos);
		s32 GetNametableIndexFromMetatilePos(const glm::ivec2& metatilePos);
		glm::ivec2 GetNametableOffsetFromMetatilePos(const glm::ivec2& metatilePos);
		void SetNametableTile(Nametable* pNametable, const glm::ivec2& nametableOffset, BgTile tileIndex);
		Metatile GetNametableMetatile(const Nametable* pNametable, u32 metatileIndex);
		void SetNametableMetatile(Nametable* pNametable, const glm::ivec2& metatileOffset, const Metatile& metatile);

		void SavePaletteToFile(const char* fname);
	}
}