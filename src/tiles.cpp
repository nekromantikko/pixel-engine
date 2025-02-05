#include "tiles.h"
#include "rendering_util.h"
#include "system.h"

#pragma region Tileset
static inline u32 GetTilesetAttributeIndex(u32 tileIndex) {
	const u32 x = tileIndex & (TILESET_DIM - 1);
	const u32 y = tileIndex >> TILESET_DIM_LOG2;

	return (x >> 1) + (y >> 1) * TILESET_DIM_ATTRIBUTES;
}

static inline u32 GetTilesetAttributeOffset(u32 tileIndex) {
	const u32 y = tileIndex >> TILESET_DIM_LOG2;

	return (tileIndex & 1) + (y & 1) * 2;
}

s32 Tiles::GetTilesetPalette(const Tileset* tileset, u32 tileIndex) {
	if (tileIndex > TILESET_SIZE) {
		return -1;
	}

	const s32 index = GetTilesetAttributeIndex(tileIndex);
	const s8 offset = GetTilesetAttributeOffset(tileIndex);

	u8 attribute = tileset->attributes[index];
	return Rendering::Util::GetPalette(attribute, offset);
}

bool Tiles::SetTilesetPalette(Tileset* tileset, u32 tileIndex, s32 palette) {
	if (tileIndex > TILESET_SIZE) {
		return false;
	}

	const s32 index = GetTilesetAttributeIndex(tileIndex);
	const s8 offset = GetTilesetAttributeOffset(tileIndex);

	u8& attribute = tileset->attributes[index];
	attribute = Rendering::Util::SetPalette(attribute, offset, palette);

	return true;
}
#pragma endregion

#pragma region New API
bool Tiles::TileInMapBounds(const Tilemap* pTilemap, const IVec2& pos) {
    if (pos.x < 0 || pos.y < 0) {
        return false;
    }

    const s32 xMax = pTilemap->width * VIEWPORT_WIDTH_METATILES;
    const s32 yMax = pTilemap->height * VIEWPORT_HEIGHT_METATILES;

    if (pos.x >= xMax || pos.y >= yMax) {
        return false;
    }

    return true;
}

static s32 GetScreenIndex(const Tilemap* pTilemap, const IVec2& pos) {
    return (pos.x / VIEWPORT_WIDTH_METATILES) + (pos.y / VIEWPORT_HEIGHT_METATILES) * pTilemap->width;
}

static s32 GetTileIndex(const Tilemap* pTilemap, const IVec2& pos) {
    return (pos.x % VIEWPORT_WIDTH_METATILES) + (pos.y % VIEWPORT_HEIGHT_METATILES) * VIEWPORT_WIDTH_METATILES;
}

s32 Tiles::GetTilesetIndex(const Tilemap* pTilemap, const IVec2& pos) {
    if (!TileInMapBounds(pTilemap, pos)) {
        return -1;
    }

    const s32 screenIndex = GetScreenIndex(pTilemap, pos);
    const s32 i = GetTileIndex(pTilemap, pos);

    return pTilemap->pScreens[screenIndex].tiles[i];
}

const MapTile* Tiles::GetMapTile(const Tilemap* pTilemap, const s32& tilesetIndex) {
    if (tilesetIndex == -1) {
        return nullptr;
    }

    return &pTilemap->pTileset->tiles[tilesetIndex];
}

const MapTile* Tiles::GetMapTile(const Tilemap* pTilemap, const IVec2& pos) {
    s32 index = GetTilesetIndex(pTilemap, pos);
    return GetMapTile(pTilemap, index);
}

bool Tiles::SetMapTile(const Tilemap* pTilemap, s32 screenIndex, s32 tileIndex, const s32& tilesetIndex) {
    if (tilesetIndex == -1) {
        return false;
    }

    pTilemap->pScreens[screenIndex].tiles[tileIndex] = tilesetIndex;
    return true;
}

bool Tiles::SetMapTile(const Tilemap* pTilemap, const IVec2& pos, const s32& tilesetIndex) {
    if (!TileInMapBounds(pTilemap, pos)) {
        return false;
    }

    const s32 screenIndex = GetScreenIndex(pTilemap, pos);
    const s32 i = GetTileIndex(pTilemap, pos);

    return SetMapTile(pTilemap, screenIndex, i, tilesetIndex);
}

s32 Tiles::GetNametableIndex(const IVec2& pos) {
    return (pos.x / NAMETABLE_WIDTH_METATILES + pos.y / NAMETABLE_HEIGHT_METATILES) % NAMETABLE_COUNT;
}

IVec2 Tiles::GetNametableOffset(const IVec2& pos) {
    return { (s32)(pos.x % NAMETABLE_WIDTH_METATILES), (s32)(pos.y % NAMETABLE_HEIGHT_METATILES) };
}
#pragma endregion

static constexpr u32 tilesetMaxNameLength = 256;
static char name[tilesetMaxNameLength];

static Tileset tileset;

void Tiles::LoadTileset(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "rb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to load tile collision file\n");
	}

	const char signature[4]{};
	fread((void*)signature, sizeof(u8), 4, pFile);
	fread((void*)name, sizeof(char), tilesetMaxNameLength, pFile);
	fread((void*)&tileset.tiles, sizeof(MapTile), TILESET_SIZE, pFile);
	fread((void*)&tileset.attributes, sizeof(u8), TILESET_ATTRIBUTE_COUNT, pFile);

	fclose(pFile);
}

void Tiles::SaveTileset(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "wb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to write tileset file\n");
	}

	const char signature[4] = "TIL";
	fwrite(signature, sizeof(u8), 4, pFile);
	fwrite(name, sizeof(char), tilesetMaxNameLength, pFile);
	fwrite(&tileset.tiles, sizeof(MapTile), TILESET_SIZE, pFile);
	fwrite(&tileset.attributes, sizeof(u8), TILESET_ATTRIBUTE_COUNT, pFile);

	fclose(pFile);
}

Tileset* Tiles::GetTileset() {
	return &tileset;
}