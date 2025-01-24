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
            level.width = 1;
            level.height = 1;
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
            fread(&level.type, sizeof(LevelType), 1, pFile);
            fread(&level.flags, sizeof(LevelFlagBits), 1, pFile);
            fread(&level.width, sizeof(u32), 1, pFile);
            fread(&level.height, sizeof(u32), 1, pFile);
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
            fwrite(&level.type, sizeof(LevelType), 1, pFile);
            fwrite(&level.flags, sizeof(LevelFlagBits), 1, pFile);
            fwrite(&level.width, sizeof(u32), 1, pFile);
            fwrite(&level.height, sizeof(u32), 1, pFile);
        }

        fwrite(nameMemory, levelMaxNameLength, maxLevelCount, pFile);
        fwrite(screenMemory, levelMaxScreenCount * sizeof(Screen), maxLevelCount, pFile);

        fclose(pFile);
    }

    // Potentially heavy operation
    bool SwapLevels(u32 a, u32 b) {
        if (a > maxLevelCount || b > maxLevelCount) {
            return false;
        }

        // No need to swap, task failed successfully
        if (a == b) {
            return true;
        }

        Level& levelA = levels[a];
        Screen* levelAScreens = &screenMemory[levelMaxScreenCount * a];
        char* levelAName = &nameMemory[levelMaxNameLength * a];
        Level& levelB = levels[b];
        Screen* levelBScreens = &screenMemory[levelMaxScreenCount * b];
        char* levelBName = &nameMemory[levelMaxNameLength * b];

        // Copy A to temp
        Level temp = levelA;
        temp.screens = levelBScreens;
        temp.name = levelBName;

        void* tempScreens = calloc(sizeof(Screen), levelMaxScreenCount);
        void* tempName = calloc(sizeof(char), levelMaxNameLength);

        memcpy(tempScreens, levelAScreens, levelMaxScreenCount * sizeof(Screen));
        memcpy(tempName, levelAName, levelMaxNameLength);

        // Copy B to A
        levelA = levelB;
        levelA.screens = levelAScreens;
        levelA.name = levelAName;
        memcpy(levelAScreens, levelBScreens, levelMaxScreenCount * sizeof(Screen));
        memcpy(levelAName, levelBName, levelMaxNameLength);

        // Copy Temp to B
        levelB = temp;
        levelB.screens = levelBScreens;
        levelB.name = levelBName;
        memcpy(levelBScreens, tempScreens, levelMaxScreenCount * sizeof(Screen));
        memcpy(levelBName, tempName, levelMaxNameLength);

        free(tempScreens);
        free(tempName);
    }

    // UTILS
    Vec2 ScreenOffsetToWorld(const Level* pLevel, Vec2 screenOffset, u32 screenIndex) {
        const Vec2 screenWorld = ScreenIndexToWorld(pLevel, screenIndex);
        return screenWorld + screenOffset;
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
    u32 WorldToNametableIndex(Vec2 world) {
        const IVec2 tilemap = WorldToTilemap(world);
        return TilemapToNametableIndex(tilemap);
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
    Vec2 TilemapToScreenOffset(IVec2 tilemap) {
        return {
            (float)(tilemap.x % screenWidthMetatiles),
            (float)(tilemap.y % screenHeightMetatiles)
        };
    }
    u32 TilemapToScreenIndex(const Level *pLevel, IVec2 tilemap) {
        return (tilemap.x / screenWidthMetatiles) + (tilemap.y / screenHeightMetatiles) * pLevel->width;
    }
    u32 TilemapToMetatileIndex(IVec2 tilemap) {
        // Eliminate negative coordinates:
        s32 x = tilemap.x;
        while (x < 0) {
            x += screenWidthMetatiles;
        }

        s32 y = tilemap.y;
        while (y < 0) {
            y += screenHeightMetatiles;
        }

        const u32 screenRelativeX = x % screenWidthMetatiles;
        const u32 screenRelativeY = y % screenHeightMetatiles;
        return screenRelativeY * screenWidthMetatiles + screenRelativeX;
    }
    u32 TilemapToNametableIndex(IVec2 tilemap) {
        return (tilemap.x / nametableWidthMetatiles + tilemap.y / nametableHeightMetatiles) % NAMETABLE_COUNT;
    }

    Vec2 TileIndexToScreenOffset(u32 tileIndex) {
        return {
        (r32)((tileIndex % screenWidthMetatiles) * Tileset::metatileWorldSize),
        (r32)((tileIndex / screenWidthMetatiles) * Tileset::metatileWorldSize),
        };
    }

    Vec2 ScreenIndexToWorld(const Level* pLevel, u32 screenIndex) {
        return {
            (r32)(screenIndex % pLevel->width) * screenWidthTiles,
            (r32)(screenIndex / pLevel->width) * screenHeightTiles
        };
    }

    bool TileInLevelBounds(const Level* pLevel, IVec2 tilemapCoord) {
        if (tilemapCoord.x < 0 || tilemapCoord.y < 0) {
            return false;
        }

        s32 xMax = pLevel->width * screenWidthMetatiles;
        r32 yMax = pLevel->height * screenHeightMetatiles;

        if (tilemapCoord.x >= xMax || tilemapCoord.y >= yMax) {
            return false;
        }

        return true;
    }
}

