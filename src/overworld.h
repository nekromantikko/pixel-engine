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
	Tilemap tilemap;
	OverworldKeyArea keyAreas[MAX_OVERWORLD_KEY_AREA_COUNT];
};