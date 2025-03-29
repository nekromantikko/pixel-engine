#pragma once
#include "typedef.h"
#include "tiles.h"
#include "asset_types.h"

constexpr u32 MAX_DUNGEON_COUNT = 64;
constexpr u32 DUNGEON_GRID_DIM = 32;
constexpr u32 DUNGEON_GRID_SIZE = DUNGEON_GRID_DIM * DUNGEON_GRID_DIM;
constexpr u32 DUNGEON_MAX_NAME_LENGTH = 256;
constexpr u32 MAX_DUNGEON_ROOM_COUNT = 128;

struct RoomInstance {
	u32 id;
	s32 templateIndex;
};

struct DungeonCell {
	s8 roomIndex = -1;
	u8 screenIndex = 0;
};

struct Dungeon {
	//char* name;
	u32 roomCount;
	RoomInstance rooms[MAX_DUNGEON_ROOM_COUNT];
	DungeonCell grid[DUNGEON_GRID_SIZE];
};

struct RoomInstanceNew {
	u32 id;
	RoomTemplateHandle templateId;
};

struct DungeonNew {
	u32 roomCount;
	RoomInstanceNew rooms[MAX_DUNGEON_ROOM_COUNT];
	DungeonCell grid[DUNGEON_GRID_SIZE];
};

namespace Assets {
	Dungeon* GetDungeon(s32 index);
	
	void LoadDungeons(const char* fname);
	void SaveDungeons(const char* fname);
}