#include "level.h"
#include "system.h"
#include <cstdio>
#include <cstring>

static Level levels[MAX_LEVEL_COUNT];
static char nameMemory[MAX_LEVEL_COUNT * LEVEL_MAX_NAME_LENGTH];
static Screen screenMemory[MAX_LEVEL_COUNT * LEVEL_MAX_SCREEN_COUNT];

void Levels::Init() {
    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        Level& level = levels[i];
        level.actors.Init(LEVEL_MAX_ACTOR_COUNT);
    }
}

Level* Levels::GetLevelsPtr() {
    return levels;
}

void Levels::InitializeLevels() {
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

void Levels::LoadLevels(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to load level file\n");
    }

    char signature[4]{};
    fread(signature, sizeof(u8), 4, pFile);

    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        Level& level = levels[i];
        fread(&level.unused1, sizeof(u32), 1, pFile);
        fread(&level.unused2, sizeof(u32), 1, pFile);
        fread(&level.tilemap.width, sizeof(s32), 1, pFile);
        fread(&level.tilemap.height, sizeof(s32), 1, pFile);
    }

    fread(nameMemory, LEVEL_MAX_NAME_LENGTH, MAX_LEVEL_COUNT, pFile);
    fread(screenMemory, LEVEL_MAX_SCREEN_COUNT * sizeof(Screen), MAX_LEVEL_COUNT, pFile);

    fclose(pFile);

    // Init references
    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        Level& level = levels[i];
        level.name = &nameMemory[i * LEVEL_MAX_NAME_LENGTH];
        level.tilemap.pTileset = Tiles::GetTileset();
        level.tilemap.pScreens = &screenMemory[i * LEVEL_MAX_SCREEN_COUNT];

        level.actors.Clear();
    }
}

void Levels::SaveLevels(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to write level file\n");
    }

    const char signature[4] = "LEV";
    fwrite(signature, sizeof(u8), 4, pFile);

    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        const Level& level = levels[i];
        fwrite(&level.unused1, sizeof(u32), 1, pFile);
        fwrite(&level.unused2, sizeof(u32), 1, pFile);
        fwrite(&level.tilemap.width, sizeof(u32), 1, pFile);
        fwrite(&level.tilemap.height, sizeof(u32), 1, pFile);
    }

    fwrite(nameMemory, LEVEL_MAX_NAME_LENGTH, MAX_LEVEL_COUNT, pFile);
    fwrite(screenMemory, LEVEL_MAX_SCREEN_COUNT * sizeof(Screen), MAX_LEVEL_COUNT, pFile);

    fclose(pFile);
}

