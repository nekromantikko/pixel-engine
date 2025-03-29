#include "dungeon.h"
#include "debug.h"
#include "asset_manager.h"
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

    /*for (u32 i = 0; i < 4; i++) {
        Dungeon& dungeon = dungeons[i];

        DungeonHandle dungeonId = AssetManager::CreateAsset<ASSET_TYPE_DUNGEON>(sizeof(DungeonNew), "Untitled Dungeon");
        DungeonNew* pNew = (DungeonNew*)AssetManager::GetAsset(dungeonId);

        pNew->roomCount = dungeon.roomCount;
        memcpy(pNew->grid, dungeon.grid, sizeof(dungeon.grid));
        for (u32 j = 0; j < MAX_DUNGEON_ROOM_COUNT; j++) {
            RoomInstance& old = dungeon.rooms[j];
            RoomInstanceNew& njew = pNew->rooms[j];
            njew.id = old.id;
            njew.templateId = 0;
        }
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