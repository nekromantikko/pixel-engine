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
    ActorPrototypeHandle prototypeId;
    glm::vec2 position;
};

struct RoomTemplateHeader {
    u8 width;
    u8 height;
    u32 mapTileOffset;
    Tilemap tilemapHeader;
    u32 actorCount;
    u32 actorOffset;
};

namespace Assets {
    void InitRoomTemplate(u64 id, void* data);

    u32 GetRoomTemplateSize(const RoomTemplateHeader* pHeader = nullptr);
    BgTile* GetRoomTemplateMapTiles(const RoomTemplateHeader* pHeader);
    RoomActor* GetRoomTemplateActors(const RoomTemplateHeader* pHeader);
}
