#include "level.h"
#include <stdio.h>

void LoadLevel(Level* pLevel, const char* fname)
{
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        ERROR("Failed to load level file\n");
    }

    const char signature[4]{};
    fread((void*)signature, sizeof(u8), 4, pFile);
    LevelFlagBits flags;
    fread(&flags, sizeof(LevelFlagBits), 1, pFile);
    u32 screenCount;
    fread(&screenCount, sizeof(u32), 1, pFile);
    Screen* screens = (Screen*)calloc(screenCount, sizeof(Screen));
    fread((void*)screens, sizeof(Screen), screenCount, pFile);

    pLevel->name = fname;
    pLevel->flags = flags;
    pLevel->screenCount = screenCount;
    pLevel->screens = screens;

    fclose(pFile);
}

void SaveLevel(Level* pLevel, const char* fname)
{
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        ERROR("Failed to write level file\n");
    }

    const char signature[4] = "LEV";
    fwrite(signature, sizeof(u8), 4, pFile);
    fwrite(&pLevel->flags, sizeof(LevelFlagBits), 1, pFile);
    fwrite(&pLevel->screenCount, sizeof(u32), 1, pFile);
    fwrite(pLevel->screens, sizeof(Screen), pLevel->screenCount, pFile);

    fclose(pFile);
}

// UTILS
Vec2 ScreenOffsetToWorld(const Level* pLevel, Vec2 screenOffset, u32 screenIndex) {
    if (pLevel->flags & LFLAGS_SCROLL_VERTICAL) {
        return {
            screenOffset.x,
            screenOffset.y + screenIndex * LEVEL_SCREEN_HEIGHT_TILES
        };
    }
    else {
        return {
            screenOffset.x + screenIndex * LEVEL_SCREEN_WIDTH_TILES,
            screenOffset.y
        };
    }
}
IVec2 ScreenOffsetToTilemap(const Level* pLevel, Vec2 screenOffset, u32 screenIndex) {
    const Vec2 world = ScreenOffsetToWorld(pLevel, screenOffset, screenIndex);
    return WorldToTilemap(world);
}
u32 ScreenOffsetToMetatileIndex(const Level* pLevel, Vec2 screenOffset) {
    const IVec2 tilemap = ScreenOffsetToTilemap(pLevel, screenOffset, 0);
    return TilemapToMetatileIndex(tilemap);
}

s32 WorldToTilemap(r32 world) {
    return s32(world / Tileset::metatileWorldSize);
}
IVec2 WorldToTilemap(Vec2 world) {
    return {
        WorldToTilemap(world.x),
        WorldToTilemap(world.y)
    };
}
u32 WorldToScreenIndex(const Level* pLevel, Vec2 world) {
    const IVec2 tilemap = WorldToTilemap(world);
    return TilemapToScreenIndex(pLevel, tilemap);
}
Vec2 WorldToScreenOffset(Vec2 world) {
    return {
        fmodf(world.x, LEVEL_SCREEN_WIDTH_TILES),
        fmodf(world.y, LEVEL_SCREEN_HEIGHT_TILES)
    };
}
u32 WorldToMetatileIndex(Vec2 world) {
    const IVec2 tilemap = WorldToTilemap(world);
    return TilemapToMetatileIndex(tilemap);
}

r32 TilemapToWorld(s32 tilemap) {
    return tilemap * (s32)Tileset::metatileWorldSize;
}
Vec2 TilemapToWorld(IVec2 tilemap) {
    return {
        TilemapToWorld(tilemap.x),
        TilemapToWorld(tilemap.y)
    };
}
u32 TilemapToScreenIndex(const Level *pLevel, IVec2 tilemap) {
    if (pLevel->flags & LFLAGS_SCROLL_VERTICAL) {
        return tilemap.y / LEVEL_SCREEN_HEIGHT_METATILES;
    }
    else {
        return tilemap.x / LEVEL_SCREEN_WIDTH_METATILES;
    }
}
u32 TilemapToMetatileIndex(IVec2 tilemap) {
    u32 screenRelativeX = tilemap.x % LEVEL_SCREEN_WIDTH_METATILES;
    u32 screenRelativeY = tilemap.y % LEVEL_SCREEN_HEIGHT_METATILES;
    return screenRelativeY * LEVEL_SCREEN_WIDTH_METATILES + screenRelativeX;
}

Vec2 TileIndexToScreenOffset(u32 tileIndex) {
    return {
    (r32)((tileIndex % LEVEL_SCREEN_WIDTH_METATILES) * Tileset::metatileWorldSize),
    (r32)((tileIndex / LEVEL_SCREEN_WIDTH_METATILES) * Tileset::metatileWorldSize),
    };
}

Vec2 ScreenIndexToWorld(const Level* pLevel, u32 screenIndex) {
    if (pLevel->flags & LFLAGS_SCROLL_VERTICAL) {
        return {
            0.0f,
            (r32)screenIndex* LEVEL_SCREEN_HEIGHT_TILES,
        };
    }
    else {
        return {
            (r32)screenIndex * LEVEL_SCREEN_WIDTH_TILES,
            0.0f
        };
    }
}
