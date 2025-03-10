#pragma once
#include "typedef.h"
#include "tiles.h"

constexpr u32 DUNGEON_GRID_DIM = 32;
constexpr u32 DUNGEON_GRID_SIZE = DUNGEON_GRID_DIM * DUNGEON_GRID_DIM;
constexpr u32 MAX_DUNGEON_ROOM_COUNT = 128;

struct Level;

struct RoomInstance {
	u32 id;
	const Level* pTemplate;
};

struct DungeonCell {
	s8 roomIndex = -1;
	u8 screenIndex = 0;
};

struct Dungeon {
	u32 roomCount;
	RoomInstance rooms[MAX_DUNGEON_ROOM_COUNT];
	DungeonCell grid[DUNGEON_GRID_SIZE];
};

namespace Assets {
	void LoadDungeon(const char* fname, Dungeon* pDungeon);
	void SaveDungeon(const char* fname, const Dungeon* pDungeon);
}