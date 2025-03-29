#include "room.h"
#include "actor_prototypes.h"
#include "debug.h"
#include "asset_manager.h"
#include <cstdio>
#include <cstring>

static RoomTemplate roomTemplates[MAX_ROOM_TEMPLATE_COUNT];
static u8 tileMemory[MAX_ROOM_TEMPLATE_COUNT * ROOM_TILE_COUNT];

struct RoomTemplatesHeader {
    char signature[4];
};

void Assets::InitRoomTemplate(void* data) {
    constexpr u32 mapTilesOffset = sizeof(RoomTemplateHeader);
    constexpr u32 tilesOffset = mapTilesOffset + ROOM_MAP_TILE_COUNT * sizeof(BgTile);
    constexpr u32 actorsOffset = tilesOffset + ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;

    TilemapHeader tilemapHeader{
        .width = ROOM_MAX_DIM_SCREENS * VIEWPORT_WIDTH_METATILES,
        .height = ROOM_MAX_DIM_SCREENS * VIEWPORT_HEIGHT_METATILES,
        .tilesetId = 0,
        .tilesOffset = tilesOffset - offsetof(RoomTemplateHeader, tilemapHeader),
    };

    RoomTemplateHeader newHeader{
        .width = 1,
        .height = 1,
        .mapTileOffset = mapTilesOffset,
        .tilemapHeader = tilemapHeader,
        .actorCount = 0,
        .actorOffset = actorsOffset
    };

    memcpy(data, &newHeader, sizeof(RoomTemplateHeader));
}

u32 Assets::GetRoomTemplateSize(const RoomTemplateHeader* pHeader) {
    u32 result = sizeof(RoomTemplateHeader);
    constexpr u32 tilemapSize = ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;
    result += tilemapSize;
    result += ROOM_MAP_TILE_COUNT * sizeof(BgTile);
    if (pHeader) {
        result += pHeader->actorCount * sizeof(RoomActorNew);
    }

    return result;
}

BgTile* Assets::GetRoomTemplateMapTiles(const RoomTemplateHeader* pHeader) {
    if (!pHeader) {
        return nullptr;
    }

    return (BgTile*)((u8*)pHeader + pHeader->mapTileOffset);
}

RoomActorNew* Assets::GetRoomTemplateActors(const RoomTemplateHeader* pHeader) {
    if (!pHeader) {
        return nullptr;
    }

    return (RoomActorNew*)((u8*)pHeader + pHeader->actorOffset);
}

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

        // Map tiles
        fread(&room.mapTiles, sizeof(BgTile), ROOM_MAX_SCREEN_COUNT * 2, pFile);
    }

    fclose(pFile);

    /*for (u32 i = 0; i < MAX_ROOM_TEMPLATE_COUNT; i++) {
        RoomTemplate& room = roomTemplates[i];

        const u32 mapTilesOffset = sizeof(RoomTemplateHeader);
        const u32 tilesOffset = mapTilesOffset + ROOM_MAP_TILE_COUNT * sizeof(BgTile);
        const u32 actorsOffset = tilesOffset + ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;

        TilemapHeader tilemapHeader{
            .width = ROOM_MAX_DIM_SCREENS * VIEWPORT_WIDTH_METATILES,
            .height = ROOM_MAX_DIM_SCREENS * VIEWPORT_HEIGHT_METATILES,
            .tilesetId = 0,
            .tilesOffset = tilesOffset - offsetof(RoomTemplateHeader, tilemapHeader),
        };

        RoomTemplateHeader newHeader{
            .width = room.width,
            .height = room.height,
            .mapTileOffset = mapTilesOffset,
            .tilemapHeader = tilemapHeader,
            .actorCount = room.actors.Count(),
            .actorOffset = actorsOffset
        };

        const u32 dataSize = GetRoomTemplateSize(&newHeader);
        RoomTemplateHandle handle = AssetManager::CreateAsset<ASSET_TYPE_ROOM_TEMPLATE>(dataSize, room.name);
        u8* data = (u8*)AssetManager::GetAsset(handle);

        memcpy(data, &newHeader, sizeof(RoomTemplateHeader));
        memcpy(data + mapTilesOffset, room.mapTiles, ROOM_MAP_TILE_COUNT * sizeof(BgTile));
        memcpy(data + tilesOffset, room.tilemap.tiles, ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT);
        for (u32 j = 0; j < room.actors.Count(); j++) {
            const RoomActor* pActor = room.actors.Get(room.actors.GetHandle(j));
            RoomActorNew* pNew = (RoomActorNew*)(data + actorsOffset + j * sizeof(RoomActorNew));
            *pNew = {
                .id = pActor->id,
                .prototypeId = 0,
                .position = pActor->position
            };
        }
    }*/

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

        // Map tiles
        fwrite(&room.mapTiles, sizeof(BgTile), ROOM_MAX_SCREEN_COUNT * 2, pFile);
    }

    fclose(pFile);
    return true;
}