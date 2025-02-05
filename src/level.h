#pragma once
#include "rendering.h"
#include "vector.h"
#include "tiles.h"
#include "actors.h"
#include "memory_pool.h"

constexpr u32 LEVEL_MAX_ACTOR_COUNT = 256;

#ifdef EDITOR
constexpr u32 LEVEL_TYPE_COUNT = 3;
constexpr const char* LEVEL_TYPE_NAMES[LEVEL_TYPE_COUNT] = { "Sidescroller", "World map", "Title screen" };
#endif

namespace Level {
    constexpr u32 maxLevelCount = 256;
    constexpr u32 levelMaxNameLength = 256;
    constexpr u32 levelMaxScreenCount = 64;

    enum LevelType : u32 {
        LTYPE_SIDESCROLLER = 0,
        LTYPE_WORLDMAP = 1,
        LTYPE_TITLESCREEN = 2
    };

    enum LevelFlagBits : u32 {
        LFLAGS_NONE = 0,
    };

    struct Level {
        char* name;
        LevelType type;
        LevelFlagBits flags;
        Tilemap tilemap;

        Pool<Actor> actors;
    };

    Level* GetLevelsPtr();

    // Generates empty data
    void InitializeLevels();

    void LoadLevels(const char* fname);
    void SaveLevels(const char* fname);
}
