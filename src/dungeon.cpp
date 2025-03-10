#include "dungeon.h"
#include "system.h"
#include "level.h"
#include <cstdio>

void Assets::LoadDungeon(const char* fname, Dungeon* pDungeon) {
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to load level file\n");
    }

    char signature[4]{};
    fread(signature, sizeof(u8), 4, pFile);

    fread(&pDungeon->roomCount, sizeof(u32), 1, pFile);
    for (u32 i = 0; i < pDungeon->roomCount; i++) {
        RoomInstance& room = pDungeon->rooms[i];

        fread(&room.id, sizeof(u32), 1, pFile);

        s32 templateIndex;
        fread(&templateIndex, sizeof(s32), 1, pFile);
        room.pTemplate = Levels::GetLevel(templateIndex);
    }

    fread(pDungeon->grid, sizeof(pDungeon->grid), 1, pFile);

    fclose(pFile);
}
void Assets::SaveDungeon(const char* fname, const Dungeon* pDungeon) {
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to write level file\n");
    }

    const char signature[4] = "DNG";
    fwrite(signature, sizeof(u8), 4, pFile);

    fwrite(&pDungeon->roomCount, sizeof(u32), 1, pFile);
    for (u32 i = 0; i < pDungeon->roomCount; i++) {
        const RoomInstance& room = pDungeon->rooms[i];
        const s32 templateIndex = Levels::GetIndex(room.pTemplate);

        fwrite(&room.id, sizeof(u32), 1, pFile);
        fwrite(&templateIndex, sizeof(s32), 1, pFile);
    }

    fwrite(pDungeon->grid, sizeof(pDungeon->grid), 1, pFile);

    fclose(pFile);
}