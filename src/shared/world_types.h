#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "rendering_types.h"
#include "actor_types.h"
#include "memory_pool.h"
#include <cassert>

// =================
// TILEMAP TYPES
// =================
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
};

// =================
// ROOM TYPES
// =================
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

// =================
// DUNGEON TYPES
// =================
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

// =================
// OVERWORLD TYPES
// =================
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