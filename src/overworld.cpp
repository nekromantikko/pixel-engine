#include "overworld.h"
#include "asset_manager.h"

void Assets::InitOverworld(u64 id, void* data) {
    constexpr u32 keyAreaOffset = sizeof(Overworld);
    constexpr u32 tilesOffset = keyAreaOffset + MAX_OVERWORLD_KEY_AREA_COUNT * sizeof(OverworldKeyArea);

    Tilemap tilemapHeader{
        .width = OVERWORLD_WIDTH_METATILES,
        .height = OVERWORLD_HEIGHT_METATILES,
        .tilesetId = TilesetHandle::Null(),
        .tilesOffset = tilesOffset - offsetof(Overworld, tilemapHeader),
    };

    Overworld newHeader{
        .tilemapHeader = tilemapHeader,
        .keyAreaOffset = keyAreaOffset,
    };

    memcpy(data, &newHeader, sizeof(Overworld));

    OverworldKeyArea* pKeyAreas = GetOverworldKeyAreas((Overworld*)data);
    for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
        pKeyAreas[i].position = { -1, -1 };
    }
}

OverworldKeyArea* Assets::GetOverworldKeyAreas(const Overworld* pHeader) {
    if (!pHeader) {
        return nullptr;
    }

    return (OverworldKeyArea*)((u8*)pHeader + pHeader->keyAreaOffset);
}

u32 Assets::GetOverworldSize() {
    u32 result = sizeof(Overworld);
    constexpr u32 tilemapSize = OVERWORLD_METATILE_COUNT;
    result += tilemapSize;
    result += MAX_OVERWORLD_KEY_AREA_COUNT * sizeof(OverworldKeyArea);
    return result;
}