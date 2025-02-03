#pragma once
#include "rendering.h"
#include "vector.h"
#include "tiles.h"

#ifdef EDITOR
constexpr u32 LEVEL_TYPE_COUNT = 3;
constexpr const char* LEVEL_TYPE_NAMES[LEVEL_TYPE_COUNT] = { "Sidescroller", "World map", "Title screen" };

constexpr u32 ACTOR_TYPE_COUNT = 3;
constexpr const char* ACTOR_TYPE_NAMES[ACTOR_TYPE_COUNT] = { "None", "Door", "Skull" };
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

    enum ActorType : u8 {
        ACTOR_NONE = 0,
        ACTOR_DOOR = 1,
        ACTOR_SKULL_ENEMY = 2,

    };

    struct LevelTile {
        u8 metatile;
        ActorType actorType;
        u8 unused1;
        u8 unused2;
    };

    struct Level {
        char* name;
        LevelType type;
        LevelFlagBits flags;
        Tilemap tilemap;
    };

    Level* GetLevelsPtr();

    // Generates empty data
    void InitializeLevels();

    void LoadLevels(const char* fname);
    void SaveLevels(const char* fname);

    bool SwapLevels(u32 a, u32 b);
}
