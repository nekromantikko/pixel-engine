#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include <cassert>
#include "typedef.h"
#include "rendering.h"

constexpr u32 TILESET_DIM = 16;
constexpr u32 TILESET_DIM_LOG2 = 4;
constexpr u32 TILESET_SIZE = TILESET_DIM * TILESET_DIM;
constexpr u32 TILESET_DIM_ATTRIBUTES = TILESET_DIM >> 1;
constexpr u32 TILESET_ATTRIBUTE_COUNT = TILESET_DIM_ATTRIBUTES * TILESET_DIM_ATTRIBUTES;

static_assert(TILESET_DIM == (1 << TILESET_DIM_LOG2));

enum MapTileType : s32 {
	TILE_EMPTY = 0,
	TILE_SOLID = 1,
	TILE_TYPE_COUNT
};

#ifdef EDITOR
constexpr const char* METATILE_TYPE_NAMES[TILE_TYPE_COUNT] = { "Empty", "Solid" };
#endif

struct MapTile {
	s32 type;
	Metatile metatile;
};

struct Tileset {
	MapTile tiles[TILESET_SIZE];
	u8 attributes[TILESET_ATTRIBUTE_COUNT];
};

struct Screen {
	u8 tiles[VIEWPORT_SIZE_METATILES];
};

struct Tilemap {
	s32 width;
	s32 height;
	Tileset* pTileset;
	Screen* pScreens;
};

namespace Tiles {
	// Tileset utils
	s32 GetTilesetPalette(const Tileset* tileset, u32 tileIndex);
	bool SetTilesetPalette(Tileset* tileset, u32 tileIndex, s32 palette);

	// New API
	bool TileInMapBounds(const Tilemap* pTilemap, const glm::ivec2& pos);
	s32 GetTilesetIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	const MapTile* GetMapTile(const Tilemap* pTilemap, const s32& tilesetIndex);
	const MapTile* GetMapTile(const Tilemap* pTilemap, const glm::ivec2& pos);
	bool SetMapTile(const Tilemap* pTilemap, s32 screenIndex, s32 tileIndex, const s32& tilesetIndex);
	bool SetMapTile(const Tilemap* pTilemap, const glm::ivec2& pos, const s32& tilesetIndex);

	s32 GetNametableIndex(const glm::ivec2& pos);
	glm::ivec2 GetNametableOffset(const glm::ivec2& pos);

	// Temp until we have an asset manager
	void LoadTileset(const char* fname);
	void SaveTileset(const char* fname);
	Tileset* GetTileset();
}