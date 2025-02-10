#pragma once
#include "rendering.h"
#include "vector.h"
#include "tiles.h"
#include "actors.h"
#include "memory_pool.h"

constexpr u32 MAX_LEVEL_COUNT = 256;
constexpr u32 LEVEL_MAX_NAME_LENGTH = 256;
constexpr u32 LEVEL_MAX_SCREEN_COUNT = 64;
constexpr u32 LEVEL_MAX_ACTOR_COUNT = 256;

struct Level {
    char* name;
    u32 unused1;
    u32 unused2;
    Tilemap tilemap;

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
