#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "tilemap_types.h"

namespace Tiles {
	bool PointInBounds(const Tilemap* pTilemap, const glm::vec2& pos);
	s32 GetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	s32 GetTilesetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	bool SetTilesetTile(const Tilemap* pTilemap, s32 tileIndex, const s32& tilesetTileIndex);
	bool SetTilesetTile(const Tilemap* pTilemap, const glm::ivec2& pos, const s32& tilesetTileIndex);
}