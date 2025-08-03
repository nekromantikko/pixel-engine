#pragma once
#include "typedef.h"
#include "tilemap_types.h"
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
