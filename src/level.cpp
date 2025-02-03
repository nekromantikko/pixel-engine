#include "level.h"
#include "system.h"
#include <cstdio>
#include <cstring>

namespace Level {

    Level levels[maxLevelCount];
    char nameMemory[maxLevelCount * levelMaxNameLength];
    Screen screenMemory[maxLevelCount * levelMaxScreenCount];

    Level* GetLevelsPtr() {
        return levels;
    }

    void InitializeLevels() {
        /*for (u32 i = 0; i < maxLevelCount; i++) {
            Level& level = levels[i];

            level.name = &nameMemory[i * levelMaxNameLength];
            nameMemory[i * levelMaxNameLength] = 0;

            level.flags = LFLAGS_NONE;

            level.screens = &screenMemory[i * levelMaxScreenCount];
            screenMemory[i * levelMaxScreenCount] = Screen{};
            level.width = 1;
            level.height = 1;
        }*/
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
            fread(&level.tilemap.width, sizeof(s32), 1, pFile);
            fread(&level.tilemap.height, sizeof(s32), 1, pFile);
        }

        fread(nameMemory, levelMaxNameLength, maxLevelCount, pFile);
        fread(screenMemory, levelMaxScreenCount * sizeof(Screen), maxLevelCount, pFile);

        fclose(pFile);

        // Init references
        for (u32 i = 0; i < maxLevelCount; i++) {
            Level& level = levels[i];
            level.name = &nameMemory[i * levelMaxNameLength];
            level.tilemap.pTileset = Tiles::GetTileset();
            level.tilemap.pScreens = &screenMemory[i * levelMaxScreenCount];
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
            fwrite(&level.tilemap.width, sizeof(u32), 1, pFile);
            fwrite(&level.tilemap.height, sizeof(u32), 1, pFile);
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
        temp.tilemap.pScreens = levelBScreens;
        temp.name = levelBName;

        void* tempScreens = calloc(sizeof(Screen), levelMaxScreenCount);
        void* tempName = calloc(sizeof(char), levelMaxNameLength);

        memcpy(tempScreens, levelAScreens, levelMaxScreenCount * sizeof(Screen));
        memcpy(tempName, levelAName, levelMaxNameLength);

        // Copy B to A
        levelA = levelB;
        levelA.tilemap.pScreens = levelAScreens;
        levelA.name = levelAName;
        memcpy(levelAScreens, levelBScreens, levelMaxScreenCount * sizeof(Screen));
        memcpy(levelAName, levelBName, levelMaxNameLength);

        // Copy Temp to B
        levelB = temp;
        levelB.tilemap.pScreens = levelBScreens;
        levelB.name = levelBName;
        memcpy(levelBScreens, tempScreens, levelMaxScreenCount * sizeof(Screen));
        memcpy(levelBName, tempName, levelMaxNameLength);

        free(tempScreens);
        free(tempName);

        return true;
    }

}

