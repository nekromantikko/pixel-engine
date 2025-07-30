#pragma once
#include "rendering.h"
#include "tiles.h"
#include "actors.h"
#include "memory_pool.h"
#include "asset_types.h"

enum RoomScreenExitDir : u8 {
    SCREEN_EXIT_DIR_RIGHT,
    SCREEN_EXIT_DIR_LEFT,
    SCREEN_EXIT_DIR_BOTTOM,
    SCREEN_EXIT_DIR_TOP,

    SCREEN_EXIT_DIR_DEATH_WARP,
};

constexpr u32 ROOM_MAX_DIM_SCREENS = 4;
constexpr u32 ROOM_MAX_SCREEN_COUNT = ROOM_MAX_DIM_SCREENS * ROOM_MAX_DIM_SCREENS;
constexpr u32 ROOM_SCREEN_TILE_COUNT = VIEWPORT_WIDTH_METATILES * VIEWPORT_HEIGHT_METATILES;
constexpr u32 ROOM_MAP_TILE_COUNT = ROOM_MAX_SCREEN_COUNT * 2;
constexpr u32 ROOM_TILE_COUNT = ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;

struct RoomActor {
    u32 id;
    ActorPrototypeHandle prototypeHandle;
    glm::vec2 position;
};

struct RoomTemplate {
    u8 width;
    u8 height;
    u32 mapTileOffset;
    Tilemap tilemap;
    u32 actorCount;
    u32 actorOffset;

	inline u32 GetMapTileCount() const {
		return width * height * 2; // 2 for each screen
	}

	inline BgTile* GetMapTiles() const {
		return (BgTile*)((u8*)this + mapTileOffset);
	}

	inline RoomActor* GetActors() const {
		return (RoomActor*)((u8*)this + actorOffset);
	}
};

namespace Assets {
    void InitRoomTemplate(u64 id, void* data);

    u32 GetRoomTemplateSize(const RoomTemplate* pHeader = nullptr);
}

#ifdef EDITOR
#include <nlohmann/json.hpp>

inline void from_json(const nlohmann::json& j, RoomActor& actor) {
	j.at("id").get_to(actor.id);
	j.at("prototype_id").get_to(actor.prototypeHandle.id);
    j.at("x").get_to(actor.position.x);
	j.at("y").get_to(actor.position.y);
}

inline void to_json(nlohmann::json& j, const RoomActor& actor) {
	j["id"] = actor.id;
	j["prototype_id"] = actor.prototypeHandle.id;
	j["x"] = actor.position.x;
	j["y"] = actor.position.y;
}

inline void from_json(const nlohmann::json& j, RoomTemplate& room) {
	// TODO
}

inline void to_json(nlohmann::json& j, const RoomTemplate& room) {
	j["width"] = room.width;
	j["height"] = room.height;

	j["map_tiles"] = nlohmann::json::array();
	BgTile* mapTiles = room.GetMapTiles();
	const u32 mapTileCount = ROOM_MAP_TILE_COUNT;
	for (u32 i = 0; i < mapTileCount; ++i) {
		j["map_tiles"].push_back(mapTiles[i]);
	}

	j["tilemap"] = room.tilemap;

	RoomActor* actors = room.GetActors();
	j["actors"] = nlohmann::json::array();
	for (u32 i = 0; i < room.actorCount; ++i) {
		j["actors"].push_back(actors[i]);
	}
}

#endif

