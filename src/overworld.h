#pragma once
#include "typedef.h"
#include "tiles.h"
#include "asset_types.h"

constexpr u32 OVERWORLD_WIDTH_METATILES = 128;
constexpr u32 OVERWORLD_HEIGHT_METATILES = 128;
constexpr u32 OVERWORLD_METATILE_COUNT = OVERWORLD_WIDTH_METATILES * OVERWORLD_HEIGHT_METATILES;
constexpr u32 MAX_OVERWORLD_KEY_AREA_COUNT = 64;

struct OverworldKeyAreaFlags {
	u8 flipDirection : 1;
	u8 passthrough : 1;
};

struct OverworldKeyArea {
	DungeonHandle dungeonId;
	glm::i8vec2 position = { -1, -1 };
	glm::i8vec2 targetGridCell = { 0, 0 };
	OverworldKeyAreaFlags flags;
};

struct Overworld {
	Tilemap tilemapHeader;
	u32 keyAreaOffset;
};

namespace Assets {
	void InitOverworld(u64 id, void* data);
	OverworldKeyArea* GetOverworldKeyAreas(const Overworld* pHeader);
	u32 GetOverworldSize();
}

#ifdef EDITOR
#include <nlohmann/json.hpp>

inline void from_json(const nlohmann::json& j, OverworldKeyArea& area) {
	j.at("dungeon_id").get_to(area.dungeonId.id);
	j.at("x").get_to(area.position.x);
	j.at("y").get_to(area.position.y);
	j.at("target_x").get_to(area.targetGridCell.x);
	j.at("target_y").get_to(area.targetGridCell.y);
	area.flags.flipDirection = j.value("flip_direction", 0);
	area.flags.passthrough = j.value("passthrough", 0);
}

inline void to_json(nlohmann::json& j, const OverworldKeyArea& area) {
	j["dungeon_id"] = area.dungeonId.id;
	j["x"] = area.position.x;
	j["y"] = area.position.y;
	j["target_x"] = area.targetGridCell.x;
	j["target_y"] = area.targetGridCell.y;
	j["flip_direction"] = area.flags.flipDirection != 0;
	j["passthrough"] = area.flags.passthrough != 0;
}

inline void to_json(nlohmann::json& j, const Overworld& overworld) {
	j["tilemap"] = overworld.tilemapHeader;
	j["key_areas"] = nlohmann::json::array();
	OverworldKeyArea* keyAreas = Assets::GetOverworldKeyAreas(&overworld);
	for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; ++i) {
		j["key_areas"].push_back(keyAreas[i]);
	}
}

inline void from_json(const nlohmann::json& j, Overworld& overworld) {
	// TODO
}


#endif