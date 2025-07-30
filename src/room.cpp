#include "room.h"
#include "asset_manager.h"

void Assets::InitRoomTemplate(u64 id, void* data) {
    constexpr u32 mapTilesOffset = sizeof(RoomTemplate);
    constexpr u32 tilesOffset = mapTilesOffset + ROOM_MAP_TILE_COUNT * sizeof(BgTile);
    constexpr u32 actorsOffset = tilesOffset + ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;

    Tilemap tilemap{
        .width = ROOM_MAX_DIM_SCREENS * VIEWPORT_WIDTH_METATILES,
        .height = ROOM_MAX_DIM_SCREENS * VIEWPORT_HEIGHT_METATILES,
        .tilesetHandle = TilesetHandle::Null(),
        .tilesOffset = tilesOffset - offsetof(RoomTemplate, tilemap),
    };

    RoomTemplate newHeader{
        .width = 1,
        .height = 1,
        .mapTileOffset = mapTilesOffset,
        .tilemap = tilemap,
        .actorCount = 0,
        .actorOffset = actorsOffset
    };

    memcpy(data, &newHeader, sizeof(RoomTemplate));
}

u32 Assets::GetRoomTemplateSize(const RoomTemplate* pHeader) {
    u32 result = sizeof(RoomTemplate);
    constexpr u32 tilemapSize = ROOM_MAX_SCREEN_COUNT * ROOM_SCREEN_TILE_COUNT;
    result += tilemapSize;
    result += ROOM_MAP_TILE_COUNT * sizeof(BgTile);
    if (pHeader) {
        result += pHeader->actorCount * sizeof(RoomActor);
    }

    return result;
}