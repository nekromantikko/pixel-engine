#include "level.h"
#include "system.h"
#include <stdio.h>

namespace Level {

    Level levels[maxLevelCount];
    char nameMemory[maxLevelCount * levelMaxNameLength];
    Screen screenMemory[maxLevelCount * levelMaxScreenCount];

    Level* GetLevelsPtr() {
        return levels;
    }

    void InitializeLevels() {
        for (u32 i = 0; i < maxLevelCount; i++) {
            Level& level = levels[i];

            level.name = &nameMemory[i * levelMaxNameLength];
            nameMemory[i * levelMaxNameLength] = 0;

            level.flags = LFLAGS_NONE;

            level.screens = &screenMemory[i * levelMaxScreenCount];
            screenMemory[i * levelMaxScreenCount] = Screen{};
            level.screenCount = 1;
        }
    }

    void LoadLevels(const char* fname) {
        FILE* pFile;
        fopen_s(&pFile, fname, "rb");

        if (pFile == NULL) {
            DEBUG_ERROR("Failed to load level file\n");
        }

        char signature[4]{};
        fread(signature, sizeof(u8), 4, pFile);

        for (u32 i = 0; i < maxLevelCount; i++) {
            Level& level = levels[i];
            fread(&level.flags, sizeof(LevelFlagBits), 1, pFile);
            fread(&level.screenCount, sizeof(u32), 1, pFile);
        }

        fread(nameMemory, levelMaxNameLength, maxLevelCount, pFile);
        fread(screenMemory, levelMaxScreenCount * sizeof(Screen), maxLevelCount, pFile);

        fclose(pFile);

        // Init references
        for (u32 i = 0; i < maxLevelCount; i++) {
            Level& level = levels[i];
            level.name = &nameMemory[i * levelMaxNameLength];
            level.screens = &screenMemory[i * levelMaxScreenCount];
        }
    }

    void SaveLevels(const char* fname) {
        FILE* pFile;
        fopen_s(&pFile, fname, "wb");

        if (pFile == NULL) {
            DEBUG_ERROR("Failed to write level file\n");
        }

        const char signature[4] = "LEV";
        fwrite(signature, sizeof(u8), 4, pFile);

        for (u32 i = 0; i < maxLevelCount; i++) {
            const Level& level = levels[i];
            fwrite(&level.flags, sizeof(LevelFlagBits), 1, pFile);
            fwrite(&level.screenCount, sizeof(u32), 1, pFile);
        }

        fwrite(nameMemory, levelMaxNameLength, maxLevelCount, pFile);
        fwrite(screenMemory, levelMaxScreenCount * sizeof(Screen), maxLevelCount, pFile);

        fclose(pFile);
    }

    // UTILS
    Vec2 ScreenOffsetToWorld(const Level* pLevel, Vec2 screenOffset, u32 screenIndex) {
        if (pLevel->flags & LFLAGS_SCROLL_VERTICAL) {
            return {
                screenOffset.x,
                screenOffset.y + screenIndex * screenHeightTiles
            };
        }
        else {
            return {
                screenOffset.x + screenIndex * screenWidthTiles,
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
            fmodf(world.x, screenWidthTiles),
            fmodf(world.y, screenHeightTiles)
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
            return tilemap.y / screenHeightMetatiles;
        }
        else {
            return tilemap.x / screenWidthMetatiles;
        }
    }
    u32 TilemapToMetatileIndex(IVec2 tilemap) {
        u32 screenRelativeX = tilemap.x % screenWidthMetatiles;
        u32 screenRelativeY = tilemap.y % screenHeightMetatiles;
        return screenRelativeY * screenWidthMetatiles + screenRelativeX;
    }

    Vec2 TileIndexToScreenOffset(u32 tileIndex) {
        return {
        (r32)((tileIndex % screenWidthMetatiles) * Tileset::metatileWorldSize),
        (r32)((tileIndex / screenWidthMetatiles) * Tileset::metatileWorldSize),
        };
    }

    Vec2 ScreenIndexToWorld(const Level* pLevel, u32 screenIndex) {
        if (pLevel->flags & LFLAGS_SCROLL_VERTICAL) {
            return {
                0.0f,
                (r32)screenIndex * screenHeightTiles,
            };
        }
        else {
            return {
                (r32)screenIndex * screenWidthTiles,
                0.0f
            };
        }
    }
}

