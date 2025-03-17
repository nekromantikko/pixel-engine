#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include <cassert>
#include "typedef.h"
#include "rendering.h"
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
	u8 attributes[TILESET_ATTRIBUTE_COUNT];
};

struct TileIndexRun {
	u8 tile;
	u8 length;
};

struct Tilemap {
	u8 width;
	u8 height;
	u8 tilesetIndex;
	u8* tiles;
};

namespace Tiles {
	// Tileset utils
	s32 GetTilesetPalette(const Tileset* tileset, u32 tileIndex);
	bool SetTilesetPalette(Tileset* tileset, u32 tileIndex, s32 palette);

	// New API
	bool PointInMapBounds(const Tilemap* pTilemap, const glm::vec2& pos);
	s32 GetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	s32 GetTilesetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	const TilesetTile* GetTilesetTile(const Tilemap* pTilemap, const s32& tilesetTileIndex);
	const TilesetTile* GetTilesetTile(const Tilemap* pTilemap, const glm::ivec2& pos);
	bool SetTilesetTile(Tilemap* pTilemap, s32 tileIndex, const s32& tilesetTileIndex);
	bool SetTilesetTile(Tilemap* pTilemap, const glm::ivec2& pos, const s32& tilesetTileIndex);

	// Temp until we have an asset manager
	void LoadTileset(const char* fname);
	void SaveTileset(const char* fname);
	Tileset* GetTileset();

	// Compression
	bool CompressTiles(const u8* tiles, u32 count, std::vector<TileIndexRun>& outCompressed);
	bool DecompressTiles(const std::vector<TileIndexRun>& compressed, u8* outTiles);
}