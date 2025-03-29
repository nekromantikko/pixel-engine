#include "overworld.h"
#include "debug.h"
#include "asset_manager.h"
#include <cstdio>

static Overworld overworld;

struct OverworldHeader {
	char signature[4];
};

void Assets::InitOverworld(void* data) {
    constexpr u32 keyAreaOffset = sizeof(OverworldHeader2);
    constexpr u32 tilesOffset = keyAreaOffset + MAX_OVERWORLD_KEY_AREA_COUNT * sizeof(OverworldKeyAreaNew);

    TilemapHeader tilemapHeader{
        .width = OVERWORLD_WIDTH_METATILES,
        .height = OVERWORLD_HEIGHT_METATILES,
        .tilesetId = 0,
        .tilesOffset = tilesOffset - offsetof(OverworldHeader2, tilemapHeader),
    };

    OverworldHeader2 newHeader{
        .tilemapHeader = tilemapHeader,
        .keyAreaOffset = keyAreaOffset,
    };

    memcpy(data, &newHeader, sizeof(OverworldHeader2));

    OverworldKeyAreaNew* pKeyAreas = GetOverworldKeyAreas((OverworldHeader2*)data);
    for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
        pKeyAreas[i].position = { -1, -1 };
    }
}

OverworldKeyAreaNew* Assets::GetOverworldKeyAreas(const OverworldHeader2* pHeader) {
    if (!pHeader) {
        return nullptr;
    }

    return (OverworldKeyAreaNew*)((u8*)pHeader + pHeader->keyAreaOffset);
}

u32 Assets::GetOverworldSize() {
    u32 result = sizeof(OverworldHeader2);
    constexpr u32 tilemapSize = OVERWORLD_METATILE_COUNT;
    result += tilemapSize;
    result += MAX_OVERWORLD_KEY_AREA_COUNT * sizeof(OverworldKeyAreaNew);
    return result;
}

Overworld* Assets::GetOverworld() {
	return &overworld;
}

bool Assets::LoadOverworld(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to open overworld file\n");
        return false;
    }

    const char signature[4] = { 'W', 'R', 'L', 'D' };
    OverworldHeader header{};
    fread(&header, sizeof(OverworldHeader), 1, pFile);

    if (memcmp(signature, header.signature, 4) != 0) {
        DEBUG_ERROR("Invalid overworld file\n");
        return false;
    }

    // Tilemap
    overworld.tilemap.width = OVERWORLD_WIDTH_METATILES;
    overworld.tilemap.height = OVERWORLD_HEIGHT_METATILES;
    fread(&overworld.tilemap.tilesetIndex, sizeof(u8), 1, pFile);
    overworld.tilemap.tiles = (u8*)calloc(OVERWORLD_METATILE_COUNT, sizeof(u8));

    std::vector<TileIndexRun> compressedTiles{};
    u32 compressedTileDataSize = 0;
    fread(&compressedTileDataSize, sizeof(u32), 1, pFile);
    compressedTiles.resize(compressedTileDataSize);
    fread(compressedTiles.data(), sizeof(TileIndexRun), compressedTileDataSize, pFile);
    Tiles::DecompressTiles(compressedTiles, overworld.tilemap.tiles);

    // Key areas
    fread(overworld.keyAreas, sizeof(OverworldKeyArea), MAX_OVERWORLD_KEY_AREA_COUNT, pFile);

    fclose(pFile);

    /*OverworldHandle handle = AssetManager::CreateAsset<ASSET_TYPE_OVERWORLD>(GetOverworldSize(), "Default");
    void* data = AssetManager::GetAsset(handle);
    InitOverworld(data);
    OverworldKeyAreaNew* pKeyAreas = GetOverworldKeyAreas((OverworldHeader2*)data);
    for (u32 i = 0; i < MAX_OVERWORLD_KEY_AREA_COUNT; i++) {
        pKeyAreas[i] = {
            .dungeonId = 0,
            .position = overworld.keyAreas[i].position,
            .targetGridCell = overworld.keyAreas[i].targetGridCell,
            .flags = {
                .flipDirection = overworld.keyAreas[i].flipDirection,
                .passthrough = overworld.keyAreas[i].passthrough
}
        };
    }

    TilemapHeader* pTilemapHeader = &((OverworldHeader2*)data)->tilemapHeader;
    memcpy((u8*)pTilemapHeader + pTilemapHeader->tilesOffset, overworld.tilemap.tiles, OVERWORLD_METATILE_COUNT);*/

    return true;
}

bool Assets::SaveOverworld(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to open overworld file\n");
        return false;
    }

    OverworldHeader header = {
        .signature = { 'W', 'R', 'L', 'D' }
    };

    fwrite(&header, sizeof(OverworldHeader), 1, pFile);

    // Tilemap
    fwrite(&overworld.tilemap.tilesetIndex, sizeof(u8), 1, pFile);
    std::vector<TileIndexRun> compressedTiles{};
    Tiles::CompressTiles(overworld.tilemap.tiles, OVERWORLD_METATILE_COUNT, compressedTiles);
    const u32 compressedTileDataSize = compressedTiles.size();
    fwrite(&compressedTileDataSize, sizeof(u32), 1, pFile);
    fwrite(compressedTiles.data(), sizeof(TileIndexRun), compressedTileDataSize, pFile);

    // Key areas
    fwrite(overworld.keyAreas, sizeof(OverworldKeyArea), MAX_OVERWORLD_KEY_AREA_COUNT, pFile);

    fclose(pFile);
    return true;
}