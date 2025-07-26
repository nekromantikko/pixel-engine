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

#ifdef EDITOR
#include <nlohmann/json.hpp>

NLOHMANN_JSON_SERIALIZE_ENUM(TilesetTileType, {
	{ TILE_EMPTY, "empty" },
	{ TILE_SOLID, "solid" }
})

static void from_json(const nlohmann::json& j, BgTile& tile) {
	tile.tileId = j.at("tile_id").get<u16>();
	tile.palette = j.at("palette").get<u16>();
	tile.flipHorizontal = j.at("flip_horizontal").get<bool>();
	tile.flipVertical = j.at("flip_vertical").get<bool>();
	tile.unused = 0; // Unused bit set to 0
}

static void to_json(nlohmann::json& j, const BgTile& tile) {
	j["tile_id"] = tile.tileId;
	j["palette"] = tile.palette;
	j["flip_horizontal"] = tile.flipHorizontal != 0;
	j["flip_vertical"] = tile.flipVertical != 0;
}

static void from_json(const nlohmann::json& j, Metatile& metatile) {
	for (u32 i = 0; i < METATILE_TILE_COUNT; ++i) {
		metatile.tiles[i] = j.at("tiles").at(i).get<BgTile>();
	}
}

static void to_json(nlohmann::json& j, const Metatile& metatile) {
	j["tiles"] = nlohmann::json::array();
	for (u32 i = 0; i < METATILE_TILE_COUNT; ++i) {
		j["tiles"].push_back(metatile.tiles[i]);
	}
}

static void from_json(const nlohmann::json& j, TilesetTile& tile) {
	TilesetTileType type;
	j.at("type").get_to(type);
	tile.type = (s32)type;
	j.at("metatile").get_to(tile.metatile);
}

static void to_json(nlohmann::json& j, const TilesetTile& tile) {
	j["type"] = (TilesetTileType)tile.type;
	j["metatile"] = tile.metatile;
}

static void from_json(const nlohmann::json& j, Tileset& tileset) {
	for (u32 i = 0; i < TILESET_SIZE; ++i) {
		tileset.tiles[i] = j.at("tiles").at(i).get<TilesetTile>();
	}
}

static void to_json(nlohmann::json& j, const Tileset& tileset) {
	j["tiles"] = nlohmann::json::array();
	for (u32 i = 0; i < TILESET_SIZE; ++i) {
		j["tiles"].push_back(tileset.tiles[i]);
	}
}

#endif
