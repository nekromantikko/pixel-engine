#pragma once
#include "typedef.h"
#include "tiles.h"
#include "asset_types.h"

constexpr u32 DUNGEON_GRID_DIM = 32;
constexpr u32 DUNGEON_GRID_SIZE = DUNGEON_GRID_DIM * DUNGEON_GRID_DIM;
constexpr u32 MAX_DUNGEON_ROOM_COUNT = 128;

struct DungeonCell {
	s8 roomIndex = -1;
	u8 screenIndex = 0;
};

struct RoomInstance {
	u32 id;
	RoomTemplateHandle templateId;
};

struct Dungeon {
	u32 roomCount;
	RoomInstance rooms[MAX_DUNGEON_ROOM_COUNT];
	DungeonCell grid[DUNGEON_GRID_SIZE];
};

#ifdef EDITOR
#include <nlohmann/json.hpp>

inline void from_json(const nlohmann::json& j, DungeonCell& cell) {
	cell.roomIndex = j.at("room_index").get<s8>();
	cell.screenIndex = j.at("screen_index").get<u8>();
}

inline void to_json(nlohmann::json& j, const DungeonCell& cell) {
	j["room_index"] = cell.roomIndex;
	j["screen_index"] = cell.screenIndex;
}

inline void from_json(const nlohmann::json& j, RoomInstance& room) {
	room.id = j.at("id").get<u32>();
	room.templateId.id = j.at("template_id").get<u64>();
}

inline void to_json(nlohmann::json& j, const RoomInstance& room) {
	j["id"] = room.id;
	j["template_id"] = room.templateId.id;
}

inline void from_json(const nlohmann::json& j, Dungeon& dungeon) {
	dungeon.roomCount = j.at("rooms").size();
	for (u32 i = 0; i < dungeon.roomCount; ++i) {
		dungeon.rooms[i] = j.at("rooms").at(i).get<RoomInstance>();
	}
	for (u32 i = 0; i < DUNGEON_GRID_SIZE; ++i) {
		dungeon.grid[i] = j.at("grid").at(i).get<DungeonCell>();
	}
}

inline void to_json(nlohmann::json& j, const Dungeon& dungeon) {
	j["rooms"] = nlohmann::json::array();
	for (u32 i = 0; i < dungeon.roomCount; ++i) {
		j["rooms"].push_back(dungeon.rooms[i]);
	}
	j["grid"] = nlohmann::json::array();
	for (u32 i = 0; i < DUNGEON_GRID_SIZE; ++i) {
		j["grid"].push_back(dungeon.grid[i]);
	}
}

#endif