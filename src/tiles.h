#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include <cassert>
#include "typedef.h"
#include "rendering.h"
#include "asset_types.h"
#include <vector>

constexpr u32 TILESET_DIM = 16;
constexpr u32 TILESET_DIM_LOG2 = 4;
constexpr u32 TILESET_SIZE = TILESET_DIM * TILESET_DIM;
constexpr u32 TILESET_DIM_ATTRIBUTES = TILESET_DIM >> 1;
constexpr u32 TILESET_ATTRIBUTE_COUNT = TILESET_DIM_ATTRIBUTES * TILESET_DIM_ATTRIBUTES;

static_assert(TILESET_DIM == (1 << TILESET_DIM_LOG2));

enum TilesetTileType : s32 {
	TILE_EMPTY = 0,
	TILE_SOLID = 1,
	TILE_TYPE_COUNT
};

#ifdef EDITOR
constexpr const char* METATILE_TYPE_NAMES[TILE_TYPE_COUNT] = { "Empty", "Solid" };
#endif

struct TilesetTile {
	s32 type;
	Metatile metatile;
};

struct Tileset {
	TilesetTile tiles[TILESET_SIZE];
};

struct Tilemap {
	u32 width;
	u32 height;
	TilesetHandle tilesetId;
	u32 tilesOffset;
};

namespace Tiles {
	// New API
	bool PointInMapBounds(const Tilemap* pTilemap, const glm::vec2& pos);
	s32 GetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	s32 GetTilesetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	const TilesetTile* GetTilesetTile(const Tilemap* pTilemap, const s32& tilesetTileIndex);
	const TilesetTile* GetTilesetTile(const Tilemap* pTilemap, const glm::ivec2& pos);
	bool SetTilesetTile(Tilemap* pTilemap, s32 tileIndex, const s32& tilesetTileIndex);
	bool SetTilesetTile(Tilemap* pTilemap, const glm::ivec2& pos, const s32& tilesetTileIndex);
}

namespace Assets {
	u8* GetTilemapData(const Tilemap* pHeader);
	Tileset* GetTilemapTileset(const Tilemap* pHeader);
}