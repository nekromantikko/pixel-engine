#include "room.h"
#include "actor_prototypes.h"
#include "debug.h"
#include <cstdio>
#include <cstring>

static RoomTemplate roomTemplates[MAX_ROOM_TEMPLATE_COUNT];
static u8 tileMemory[MAX_ROOM_TEMPLATE_COUNT * ROOM_TILE_COUNT];

struct RoomTemplatesHeader {
    char signature[4];
};

RoomTemplate* Assets::GetRoomTemplate(u32 index) {
    if (index >= MAX_ROOM_TEMPLATE_COUNT) {
        return nullptr;
    }

    return &roomTemplates[index];
}

s32 Assets::GetRoomTemplateIndex(const RoomTemplate* pTemplate) {
    if (pTemplate == nullptr) {
        return -1;
    }

    s32 index = pTemplate - roomTemplates;
    if (index < 0 || index >= MAX_ROOM_TEMPLATE_COUNT) {
        return -1;
    }

    return index;
}

bool Assets::LoadRoomTemplates(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to open room template file\n");
        return false;
    }

    const char signature[4] = { 'R', 'O', 'O', 'M' };
    RoomTemplatesHeader header{};
    fread(&header, sizeof(RoomTemplatesHeader), 1, pFile);

    if (memcmp(signature, header.signature, 4) != 0) {
        DEBUG_ERROR("Invalid room template file\n");
        return false;
    }

    for (u32 i = 0; i < MAX_ROOM_TEMPLATE_COUNT; i++) {
        RoomTemplate& room = roomTemplates[i];
        fread(room.name, ROOM_MAX_NAME_LENGTH, 1, pFile);

        fread(&room.width, sizeof(u8), 1, pFile);
        fread(&room.height, sizeof(u8), 1, pFile);

        room.tilemap.width = ROOM_MAX_DIM_SCREENS * VIEWPORT_WIDTH_METATILES;
        room.tilemap.height = ROOM_MAX_DIM_SCREENS * VIEWPORT_HEIGHT_METATILES;
        fread(&room.tilemap.tilesetIndex, sizeof(u8), 1, pFile);
        u8* roomTiles = tileMemory + i * ROOM_TILE_COUNT;

        std::vector<TileIndexRun> compressedTiles{};
        u32 compressedTileDataSize = 0;
        fread(&compressedTileDataSize, sizeof(u32), 1, pFile);
        compressedTiles.resize(compressedTileDataSize);
        fread(compressedTiles.data(), sizeof(TileIndexRun), compressedTileDataSize, pFile);
        Tiles::DecompressTiles(compressedTiles, roomTiles);
        room.tilemap.tiles = roomTiles;

        // Read actors
        room.actors.Clear();
        u32 actorCount = 0;
        fread(&actorCount, sizeof(u32), 1, pFile);
        for (u32 a = 0; a < actorCount; a++) {
            RoomActor actor{};
            fread(&actor, sizeof(RoomActor), 1, pFile);
            room.actors.Add(actor);
        }
    }

    fclose(pFile);

    return true;
}

bool Assets::SaveRoomTemplates(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to open room template file\n");
        return false;
    }

    RoomTemplatesHeader header = {
        .signature = { 'R', 'O', 'O', 'M' }
    };

    fwrite(&header, sizeof(RoomTemplatesHeader), 1, pFile);

    for (u32 i = 0; i < MAX_ROOM_TEMPLATE_COUNT; i++) {
        const RoomTemplate& room = roomTemplates[i];
        fwrite(room.name, ROOM_MAX_NAME_LENGTH, 1, pFile);

        fwrite(&room.width, sizeof(u8), 1, pFile);
        fwrite(&room.height, sizeof(u8), 1, pFile);

        fwrite(&room.tilemap.tilesetIndex, sizeof(u8), 1, pFile);

        std::vector<TileIndexRun> compressedTiles{};
        Tiles::CompressTiles(room.tilemap.tiles, ROOM_TILE_COUNT, compressedTiles);
        const u32 compressedTileDataSize = compressedTiles.size();
        fwrite(&compressedTileDataSize, sizeof(u32), 1, pFile);
        fwrite(compressedTiles.data(), sizeof(TileIndexRun), compressedTileDataSize, pFile);

        // Serialize actors
        const u32 actorCount = room.actors.Count();
        fwrite(&actorCount, sizeof(u32), 1, pFile);
        for (u32 a = 0; a < actorCount; a++) {
            // TODO: what if there's a null actor somehow?
            const RoomActor* pActor = room.actors.Get(room.actors.GetHandle(a));

            fwrite(pActor, sizeof(RoomActor), 1, pFile);
        }
    }

    fclose(pFile);
    return true;
}