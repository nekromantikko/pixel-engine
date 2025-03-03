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

constexpr u32 TILEMAP_MAX_DIM_SCREENS = 4;
constexpr u32 TILEMAP_MAX_SCREEN_COUNT = TILEMAP_MAX_DIM_SCREENS * TILEMAP_MAX_DIM_SCREENS;
constexpr u32 TILEMAP_SCREEN_METADATA_SIZE = 32;
constexpr u32 TILEMAP_TILE_METADATA_SIZE = 4;

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

typedef u8 TilemapScreenMetadata[TILEMAP_SCREEN_METADATA_SIZE];
typedef u32 TilemapTileMetadata;

struct TilemapScreen {
	alignas(void*) TilemapScreenMetadata screenMetadata;

	u8 tiles[VIEWPORT_SIZE_METATILES];
	TilemapTileMetadata tileMetadata[VIEWPORT_SIZE_METATILES];
};

struct TileIndexRun {
	u8 tile;
	u8 length;
};

struct TileMetadataRun {
	TilemapTileMetadata metadata;
	alignas(4) u16 length;
};

struct TilemapScreenCompressed {
	alignas(void*) TilemapScreenMetadata screenMetadata;

	std::vector<TileIndexRun> compressedTiles;
	std::vector<TileMetadataRun> compressedMetadata;
};

struct Tilemap {
	s32 width;
	s32 height;
	Tileset* pTileset;
	TilemapScreen screens[TILEMAP_MAX_SCREEN_COUNT];
};

namespace Tiles {
	// Tileset utils
	s32 GetTilesetPalette(const Tileset* tileset, u32 tileIndex);
	bool SetTilesetPalette(Tileset* tileset, u32 tileIndex, s32 palette);

	// New API
	bool PointInMapBounds(const Tilemap* pTilemap, const glm::vec2& pos);
	s32 GetScreenIndex(const glm::ivec2& pos);
	s32 GetTileIndex(const glm::ivec2& pos);
	s32 GetTilesetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos);
	const TilesetTile* GetTilesetTile(const Tilemap* pTilemap, const s32& tilesetIndex);
	const TilesetTile* GetTilesetTile(const Tilemap* pTilemap, const glm::ivec2& pos);
	bool SetTilesetTile(Tilemap* pTilemap, s32 screenIndex, s32 tileIndex, const s32& tilesetIndex);
	bool SetTilesetTile(Tilemap* pTilemap, const glm::ivec2& pos, const s32& tilesetIndex);

	s32 GetNametableIndex(const glm::ivec2& pos);
	glm::ivec2 GetNametableOffset(const glm::ivec2& pos);

	// Temp until we have an asset manager
	void LoadTileset(const char* fname);
	void SaveTileset(const char* fname);
	Tileset* GetTileset();

	// Compression
	void CompressScreen(const TilemapScreen& screen, TilemapScreenCompressed& outCompressed);
	void DecompressScreen(const TilemapScreenCompressed& compressed, TilemapScreen& outScreen);
}