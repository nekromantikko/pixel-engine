#include "dungeon.h"
#include "debug.h"
#include "level.h"
#include <cstdio>

static Dungeon dungeons[MAX_DUNGEON_COUNT];
static char nameMemory[MAX_DUNGEON_COUNT * DUNGEON_MAX_NAME_LENGTH];

Dungeon* Assets::GetDungeon(s32 index) {
    if (index < 0 || index >= MAX_DUNGEON_COUNT) {
        return nullptr;
    }

    return &dungeons[index];
}

void Assets::LoadDungeons(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to load level file\n");
    }

    char signature[4]{};
    fread(signature, sizeof(u8), 4, pFile);

    fread(dungeons, sizeof(Dungeon), MAX_DUNGEON_COUNT, pFile);

    fclose(pFile);

    /*for (u32 i = 0; i < MAX_DUNGEON_COUNT; i++) {
        Dungeon& dungeon = dungeons[i];
        dungeon.name = &nameMemory[i * DUNGEON_MAX_NAME_LENGTH];
    }*/
}
void Assets::SaveDungeons(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to write level file\n");
    }

    const char signature[4] = "DNG";
    fwrite(signature, sizeof(u8), 4, pFile);

    fwrite(dungeons, sizeof(Dungeon), MAX_DUNGEON_COUNT, pFile);

    fclose(pFile);
}