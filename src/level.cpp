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

            level.actors.Init(LEVEL_MAX_ACTOR_COUNT);
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
}

