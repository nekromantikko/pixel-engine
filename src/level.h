#pragma once
#include "rendering.h"
#include "tileset.h"
#include "vector.h"

namespace Level {

    constexpr u32 screenWidthTiles = NAMETABLE_WIDTH_TILES;
    constexpr u32 screenWidthMetatiles = screenWidthTiles / Tileset::metatileWorldSize;
    constexpr u32 screenHeightTiles = NAMETABLE_HEIGHT_TILES;
    constexpr u32 screenHeightMetatiles = screenHeightTiles / Tileset::metatileWorldSize;

    enum LevelFlagBits : u32 {
        LFLAGS_NONE = 0,
        LFLAGS_SCROLL_VERTICAL = 1 << 0,
    };

    enum ActorType : u8 {
        ACTOR_NONE = 0,
        ACTOR_PLAYER_START = 1,

    };

    struct LevelTile {
        u8 metatile;
        ActorType actorType;
        u8 unused1;
        u8 unused2;
    };

    struct Screen {
        u32 unused1, unused2, unused3, unused4, unused5, unused6, unused7, unused8;
        LevelTile tiles[screenWidthMetatiles * screenHeightMetatiles];
    };

    struct Level {
        const char* name;
        LevelFlagBits flags;
        u32 screenCount;
        Screen* screens;
    };

    void LoadLevel(Level* pLevel, const char* fname);
    void SaveLevel(Level* pLevel, const char* fname);

    Vec2 ScreenOffsetToWorld(const Level* pLevel, Vec2 screenOffset, u32 screenIndex);
    IVec2 ScreenOffsetToTilemap(const Level* pLevel, Vec2 screenOffset, u32 screenIndex);
    u32 ScreenOffsetToMetatileIndex(const Level* pLevel, Vec2 screenOffset);

    s32 WorldToTilemap(r32 world);
    IVec2 WorldToTilemap(Vec2 world);
    u32 WorldToScreenIndex(const Level* pLevel, Vec2 world);
    Vec2 WorldToScreenOffset(Vec2 world);
    u32 WorldToMetatileIndex(Vec2 world);

    r32 TilemapToWorld(s32 tilemap);
    Vec2 TilemapToWorld(IVec2 tilemap);
    u32 TilemapToScreenIndex(const Level* pLevel, IVec2 tilemap);
    u32 TilemapToMetatileIndex(IVec2 tilemap);

    Vec2 TileIndexToScreenOffset(u32 tileIndex);

    Vec2 ScreenIndexToWorld(const Level* pLevel, u32 screenIndex);
}
