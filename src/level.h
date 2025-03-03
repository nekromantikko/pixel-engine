#pragma once
#include "rendering.h"
#include "tiles.h"
#include "actors.h"
#include "memory_pool.h"

constexpr u32 MAX_LEVEL_COUNT = 256;
constexpr u32 LEVEL_MAX_NAME_LENGTH = 256;
constexpr u32 LEVEL_MAX_ACTOR_COUNT = 256;

enum LevelScreenExitDir : u8 {
    SCREEN_EXIT_DIR_LEFT,
    SCREEN_EXIT_DIR_RIGHT,
    SCREEN_EXIT_DIR_TOP,
    SCREEN_EXIT_DIR_BOTTOM,

    SCREEN_EXIT_DIR_DEATH_WARP,
};

enum LevelType : u8 {
    LEVEL_TYPE_SIDESCROLLER = 0,
    LEVEL_TYPE_OVERWORLD,
    LEVEL_TYPE_TITLESCREEN,

    LEVEL_TYPE_COUNT
};

#ifdef EDITOR
constexpr const char* LEVEL_TYPE_NAMES[LEVEL_TYPE_COUNT] = { "Sidescroller", "Overworld", "Title screen" };
#endif

struct LevelExit {
    u16 targetLevel : 12;
    u16 targetScreen : 4;
};

struct LevelFlags {
    u8 type : 2;
};

struct Level {
    char* name;
    alignas(u32) LevelFlags flags;
    u32 unused;
    Tilemap* pTilemap;

    Pool<Actor, LEVEL_MAX_ACTOR_COUNT> actors;
};

namespace Levels {
    Level* GetLevelsPtr();
	Level* GetLevel(u32 index);
    s32 GetIndex(Level* pLevel);

    // Generates empty data
    void InitializeLevels();

    void LoadLevels(const char* fname);
    void SaveLevels(const char* fname);
}
