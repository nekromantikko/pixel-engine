#include "overworld.h"
#include "debug.h"
#include <cstdio>

static Overworld overworld;

struct OverworldHeader {
	char signature[4];
};

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