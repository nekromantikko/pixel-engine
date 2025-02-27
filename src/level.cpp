#include "level.h"
#include "system.h"
#include <cstdio>
#include <cstring>

static Level levels[MAX_LEVEL_COUNT];
static char nameMemory[MAX_LEVEL_COUNT * LEVEL_MAX_NAME_LENGTH];
static Tilemap tilemapMemory[MAX_LEVEL_COUNT];

Level* Levels::GetLevelsPtr() {
    return levels;
}

void Levels::InitializeLevels() {
    /*for (u32 i = 0; i < maxLevelCount; i++) {
        Level& level = levels[i];

        level.name = &nameMemory[i * levelMaxNameLength];
        nameMemory[i * levelMaxNameLength] = 0;

        level.flags = LFLAGS_NONE;

        level.screens = &screenMemory[i * levelMaxScreenCount];
        screenMemory[i * levelMaxScreenCount] = TilemapScreen{};
        level.width = 1;
        level.height = 1;
    }*/
}

void Levels::LoadLevels(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to load level file\n");
    }

    char signature[4]{};
    fread(signature, sizeof(u8), 4, pFile);

    // Deserialize levels
    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        Level& level = levels[i];
        level.name = &nameMemory[i * LEVEL_MAX_NAME_LENGTH];
        fread(&level.flags, sizeof(u32), 1, pFile);
        fread(&level.unused, sizeof(u32), 1, pFile);

        level.pTilemap = &tilemapMemory[i];
        level.actors.Clear();
    }

    fread(nameMemory, LEVEL_MAX_NAME_LENGTH, MAX_LEVEL_COUNT, pFile);

    // Deserialize tilemaps
    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        Tilemap& tilemap = tilemapMemory[i];

        fread(&tilemap.width, sizeof(u32), 1, pFile);
        fread(&tilemap.height, sizeof(u32), 1, pFile);

        // TODO: Multiple tilesets
        s32 tilesetIndex = 0;
        fread(&tilesetIndex, sizeof(s32), 1, pFile);
        tilemap.pTileset = Tiles::GetTileset();

        // Read compressed data
        TilemapScreenCompressed compressed{};

        for (u32 s = 0; s < TILEMAP_MAX_SCREEN_COUNT; s++) {
            TilemapScreen& screen = tilemap.screens[s];

            fread(compressed.screenMetadata, TILEMAP_SCREEN_METADATA_SIZE, 1, pFile);

            u32 compressedTileDataSize = sizeof(TileIndexRun) * compressed.compressedTiles.size();
            fread(&compressedTileDataSize, sizeof(u32), 1, pFile);

            compressed.compressedTiles.resize(compressedTileDataSize);
            fread(compressed.compressedTiles.data(), sizeof(TileIndexRun), compressed.compressedTiles.size(), pFile);

            u32 compressedMetadataSize = sizeof(TileMetadataRun) * compressed.compressedMetadata.size();
            fread(&compressedMetadataSize, sizeof(u32), 1, pFile);

            compressed.compressedMetadata.resize(compressedMetadataSize);
            fread(compressed.compressedMetadata.data(), sizeof(TileMetadataRun), compressed.compressedMetadata.size(), pFile);

            Tiles::DecompressScreen(compressed, screen);
        }
    }

    fclose(pFile);

}

void Levels::SaveLevels(const char* fname) {
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        DEBUG_ERROR("Failed to write level file\n");
    }

    const char signature[4] = "LEV";
    fwrite(signature, sizeof(u8), 4, pFile);

    // Serialize levels
    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        const Level& level = levels[i];
        fwrite(&level.flags, sizeof(u32), 1, pFile);
        fwrite(&level.unused, sizeof(u32), 1, pFile);

        // TODO: Serialize actors
    }

    fwrite(nameMemory, LEVEL_MAX_NAME_LENGTH, MAX_LEVEL_COUNT, pFile);

    // Serialize tilemaps
    for (u32 i = 0; i < MAX_LEVEL_COUNT; i++) {
        const Tilemap& tilemap = tilemapMemory[i];

        fwrite(&tilemap.width, sizeof(u32), 1, pFile);
        fwrite(&tilemap.height, sizeof(u32), 1, pFile);
        s32 tilesetIndex = 0;
        fwrite(&tilesetIndex, sizeof(s32), 1, pFile);

        // Write compressed screen data
        TilemapScreenCompressed compressed{};

        for (u32 s = 0; s < TILEMAP_MAX_SCREEN_COUNT; s++) {
            const TilemapScreen& screen = tilemap.screens[s];
            Tiles::CompressScreen(screen, compressed);

            fwrite(compressed.screenMetadata, TILEMAP_SCREEN_METADATA_SIZE, 1, pFile);
            u32 compressedTileDataSize = compressed.compressedTiles.size();
            fwrite(&compressedTileDataSize, sizeof(u32), 1, pFile);
            fwrite(compressed.compressedTiles.data(), sizeof(TileIndexRun), compressed.compressedTiles.size(), pFile);
            u32 compressedMetadataSize = compressed.compressedMetadata.size();
            fwrite(&compressedMetadataSize, sizeof(u32), 1, pFile);
            fwrite(compressed.compressedMetadata.data(), sizeof(TileMetadataRun), compressed.compressedMetadata.size(), pFile);
        }
    }

    fclose(pFile);
}

