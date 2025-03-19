#pragma once
#include "typedef.h"
#include "tiles.h"

constexpr u32 OVERWORLD_WIDTH_METATILES = 128;
constexpr u32 OVERWORLD_HEIGHT_METATILES = 128;
constexpr u32 OVERWORLD_METATILE_COUNT = OVERWORLD_WIDTH_METATILES * OVERWORLD_HEIGHT_METATILES;
constexpr u32 MAX_OVERWORLD_KEY_AREA_COUNT = 64;

struct OverworldKeyArea {
	glm::i8vec2 position = { -1, -1 };
	u8 dungeonIndex : 6;
	u8 flipDirection : 1;
};

struct Overworld {
	Tilemap tilemap;
	OverworldKeyArea keyAreas[MAX_OVERWORLD_KEY_AREA_COUNT];
};

namespace Assets {
	Overworld* GetOverworld();
	bool LoadOverworld(const char* fname);
	bool SaveOverworld(const char* fname);
}