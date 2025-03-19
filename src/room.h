#pragma once
#include "rendering.h"
#include "tiles.h"
#include "actors.h"
#include "memory_pool.h"

enum RoomScreenExitDir : u8 {
    SCREEN_EXIT_DIR_LEFT,
    SCREEN_EXIT_DIR_RIGHT,
    SCREEN_EXIT_DIR_TOP,
    SCREEN_EXIT_DIR_BOTTOM,

    SCREEN_EXIT_DIR_DEATH_WARP,
};

constexpr u32 MAX_ROOM_TEMPLATE_COUNT = 256;
constexpr u32 ROOM_MAX_NAME_LENGTH = 64;
constexpr u32 ROOM_MAX_DIM_SCREENS = 4;
constexpr u32 ROOM_MAX_SCREEN_COUNT = ROOM_MAX_DIM_SCREENS * ROOM_MAX_DIM_SCREENS;
constexpr u32 ROOM_SCREEN_TILE_COUNT = VIEWPORT_WIDTH_METATILES * VIEWPORT_HEIGHT_METATILES;
constexpr u32 ROOM_TILE_COUNT = ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;
constexpr u32 ROOM_MAX_ACTOR_COUNT = 256;

struct RoomActor {
    u32 id;
    s32 prototypeIndex;
    glm::vec2 position;
};

struct RoomTemplate {
    char name[ROOM_MAX_NAME_LENGTH];
    u8 width;
    u8 height;
    Tilemap tilemap;
    Pool<RoomActor, ROOM_MAX_ACTOR_COUNT> actors;

    BgTile mapTiles[ROOM_MAX_SCREEN_COUNT * 2];
};

namespace Assets {
    RoomTemplate* GetRoomTemplate(u32 index);
    s32 GetRoomTemplateIndex(const RoomTemplate* pTemplate);

    bool LoadRoomTemplates(const char* fname);
    bool SaveRoomTemplates(const char* fname);
}
