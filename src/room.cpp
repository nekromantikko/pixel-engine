#include "room.h"
#include "asset_manager.h"

void Assets::InitRoomTemplate(void* data) {
    constexpr u32 mapTilesOffset = sizeof(RoomTemplateHeader);
    constexpr u32 tilesOffset = mapTilesOffset + ROOM_MAP_TILE_COUNT * sizeof(BgTile);
    constexpr u32 actorsOffset = tilesOffset + ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;

    Tilemap tilemapHeader{
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
        result += pHeader->actorCount * sizeof(RoomActor);
    }

    return result;
}

BgTile* Assets::GetRoomTemplateMapTiles(const RoomTemplateHeader* pHeader) {
    if (!pHeader) {
        return nullptr;
    }

    return (BgTile*)((u8*)pHeader + pHeader->mapTileOffset);
}

RoomActor* Assets::GetRoomTemplateActors(const RoomTemplateHeader* pHeader) {
    if (!pHeader) {
        return nullptr;
    }

    return (RoomActor*)((u8*)pHeader + pHeader->actorOffset);
}