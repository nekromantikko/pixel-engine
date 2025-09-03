#include "rendering_util.h"
#include "debug.h"
#include "asset_types.h"
#include <stdio.h>
#include <gtc/constants.hpp>

namespace Rendering
{
	namespace Util
	{
#pragma region Nametable
		s32 GetNametableTileIndexFromTileOffset(const glm::ivec2& tileOffset) {
			return tileOffset.x + tileOffset.y * NAMETABLE_DIM_TILES;
		}

		s32 GetNametableTileIndexFromMetatileOffset(const glm::ivec2& metatileOffset) {
			return (metatileOffset.x << 1) + (metatileOffset.y << 1) * NAMETABLE_DIM_TILES;
		}

		s32 GetNametableIndexFromTilePos(const glm::ivec2& tilePos) {
			return (tilePos.x / NAMETABLE_DIM_TILES + tilePos.y / NAMETABLE_DIM_TILES) % NAMETABLE_COUNT;
		}

		glm::ivec2 GetNametableOffsetFromTilePos(const glm::ivec2& tilePos) {
			return { (s32)(tilePos.x % NAMETABLE_DIM_TILES), (s32)(tilePos.y % NAMETABLE_DIM_TILES) };
		}

		s32 GetNametableIndexFromMetatilePos(const glm::ivec2& pos) {
			return (pos.x / NAMETABLE_DIM_METATILES + pos.y / NAMETABLE_DIM_METATILES) % NAMETABLE_COUNT;
		}

		glm::ivec2 GetNametableOffsetFromMetatilePos(const glm::ivec2& pos) {
			return { (s32)(pos.x % NAMETABLE_DIM_METATILES), (s32)(pos.y % NAMETABLE_DIM_METATILES) };
		}

		void SetNametableTile(Nametable* pNametable, const glm::ivec2& nametableOffset, BgTile tileIndex) {
			const u32 nametableTileIndex = GetNametableTileIndexFromTileOffset(nametableOffset);

			pNametable->tiles[nametableTileIndex] = tileIndex;
		}

		Metatile GetNametableMetatile(const Nametable* pNametable, u32 metatileIndex) {
			const glm::ivec2 metatileOffset(metatileIndex % NAMETABLE_DIM_METATILES, metatileIndex / NAMETABLE_DIM_METATILES);

			const u32 firstTileIndex = GetNametableTileIndexFromMetatileOffset(metatileOffset);

			return {
				pNametable->tiles[firstTileIndex],
				pNametable->tiles[firstTileIndex + 1],
				pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES],
				pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES + 1],
			};
		}

		void SetNametableMetatile(Nametable* pNametable, const glm::ivec2& metatileOffset, const Metatile& metatile) {
			const u32 firstTileIndex = GetNametableTileIndexFromMetatileOffset(metatileOffset);

			pNametable->tiles[firstTileIndex] = metatile.tiles[0];
			pNametable->tiles[firstTileIndex + 1] = metatile.tiles[1];
			pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES] = metatile.tiles[2];
			pNametable->tiles[firstTileIndex + NAMETABLE_DIM_TILES + 1] = metatile.tiles[3];
		}
#pragma endregion
	}
}