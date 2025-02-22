#pragma once
#include "rendering.h"
#include "tiles.h"
#include "actors.h"
#include "memory_pool.h"

constexpr u32 MAX_LEVEL_COUNT = 256;
constexpr u32 LEVEL_MAX_NAME_LENGTH = 256;
constexpr u32 LEVEL_MAX_ACTOR_COUNT = 256;

enum LevelScreenExit : u8 {
    SCREEN_EXIT_MID,
    SCREEN_EXIT_LEFT,
    SCREEN_EXIT_RIGHT,
    SCREEN_EXIT_TOP,
    SCREEN_EXIT_BOTTOM,

    SCREEN_EXIT_COUNT
};

enum LevelType : u8 {
    LEVEL_TYPE_SIDESCROLLER = 0,
    LEVEL_TYPE_OVERWORLD,

    LEVEL_TYPE_COUNT
};

struct LevelExit {
    s16 targetLevel;
    u8 targetScreen;
    u8 exitType;
};

struct LevelFlags {
    u8 type : 2;
};

struct Level {
    char* name;
    alignas(u32) LevelFlags flags;
    u32 unused;
    Tilemap* pTilemap;

    Pool<Actor> actors;
};

namespace Levels {
    void Init();
    Level* GetLevelsPtr();

    // Generates empty data
    void InitializeLevels();

    void LoadLevels(const char* fname);
    void SaveLevels(const char* fname);
}
