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
	TilesetHandle tilesetHandle;
	u32 tilesOffset;

	inline u8* GetTileData() const {
		return (u8*)this + tilesOffset;
	}

	bool PointInBounds(const glm::vec2& pos) const;
	s32 GetTileIndex(const glm::ivec2& pos) const;
	s32 GetTilesetTileIndex(const glm::ivec2& pos) const;
	bool SetTilesetTile(s32 tileIndex, const s32& tilesetTileIndex) const;
	bool SetTilesetTile(const glm::ivec2& pos, const s32& tilesetTileIndex) const;
};

#ifdef EDITOR
#include <nlohmann/json.hpp>

NLOHMANN_JSON_SERIALIZE_ENUM(TilesetTileType, {
	{ TILE_EMPTY, "empty" },
	{ TILE_SOLID, "solid" }
})

inline void from_json(const nlohmann::json& j, BgTile& tile) {
	tile.tileId = j.at("tile_id").get<u16>();
	tile.palette = j.at("palette").get<u16>();
	tile.flipHorizontal = j.at("flip_horizontal").get<bool>();
	tile.flipVertical = j.at("flip_vertical").get<bool>();
	tile.unused = 0; // Unused bit set to 0
}

inline void to_json(nlohmann::json& j, const BgTile& tile) {
	j["tile_id"] = tile.tileId;
	j["palette"] = tile.palette;
	j["flip_horizontal"] = tile.flipHorizontal != 0;
	j["flip_vertical"] = tile.flipVertical != 0;
}

inline void from_json(const nlohmann::json& j, Metatile& metatile) {
	for (u32 i = 0; i < METATILE_TILE_COUNT; ++i) {
		metatile.tiles[i] = j.at("tiles").at(i).get<BgTile>();
	}
}

inline void to_json(nlohmann::json& j, const Metatile& metatile) {
	j["tiles"] = nlohmann::json::array();
	for (u32 i = 0; i < METATILE_TILE_COUNT; ++i) {
		j["tiles"].push_back(metatile.tiles[i]);
	}
}

inline void from_json(const nlohmann::json& j, TilesetTile& tile) {
	TilesetTileType type;
	j.at("type").get_to(type);
	tile.type = (s32)type;
	j.at("metatile").get_to(tile.metatile);
}

inline void to_json(nlohmann::json& j, const TilesetTile& tile) {
	j["type"] = (TilesetTileType)tile.type;
	j["metatile"] = tile.metatile;
}

inline void from_json(const nlohmann::json& j, Tileset& tileset) {
	for (u32 i = 0; i < TILESET_SIZE; ++i) {
		tileset.tiles[i] = j.at("tiles").at(i).get<TilesetTile>();
	}
}

inline void to_json(nlohmann::json& j, const Tileset& tileset) {
	j["tiles"] = nlohmann::json::array();
	for (u32 i = 0; i < TILESET_SIZE; ++i) {
		j["tiles"].push_back(tileset.tiles[i]);
	}
}

inline void to_json(nlohmann::json& j, const Tilemap& tilemap) {
	j["width"] = tilemap.width;
	j["height"] = tilemap.height;
	j["tileset_id"] = tilemap.tilesetHandle.id;
	
	u8* tilemapData = tilemap.GetTileData();
	j["tiles"] = nlohmann::json::array();
	u32 tileCount = tilemap.width * tilemap.height;
	for (u32 i = 0; i < tileCount; ++i) {
		j["tiles"].push_back(tilemapData[i]);
	}
}

inline void from_json(const nlohmann::json& j, Tilemap& tilemap) {
	// TODO
}

#endif
