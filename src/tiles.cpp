#include "tiles.h"
#include "rendering_util.h"
#include "debug.h"
#include <limits>

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
bool Tiles::PointInMapBounds(const Tilemap* pTilemap, const glm::vec2& pos) {
	if (pos.x < 0 || pos.y < 0) {
		return false;
	}

	if (pos.x >= pTilemap->width || pos.y >= pTilemap->height) {
		return false;
	}

	return true;
}

s32 Tiles::GetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos) {
	if (!PointInMapBounds(pTilemap, pos)) {
		return -1;
	}

    return (pos.x % pTilemap->width) + (pos.y % pTilemap->height) * pTilemap->width;
}

s32 Tiles::GetTilesetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos) {
    if (!PointInMapBounds(pTilemap, pos)) {
        return -1;
    }

    const s32 i = GetTileIndex(pTilemap, pos);
	if (i < 0) {
		return -1;
	}

    return pTilemap->tiles[i];
}

const TilesetTile* Tiles::GetTilesetTile(const Tilemap* pTilemap, const s32& tilesetTileIndex) {
    if (tilesetTileIndex < 0) {
        return nullptr;
    }

	const Tileset* pTileset = GetTileset();
	if (!pTileset) {
		return nullptr;
	}

    return &pTileset->tiles[tilesetTileIndex];
}

const TilesetTile* Tiles::GetTilesetTile(const Tilemap* pTilemap, const glm::ivec2& pos) {
    s32 index = GetTilesetTileIndex(pTilemap, pos);
	if (index < 0) {
		return nullptr;
	}

    return GetTilesetTile(pTilemap, index);
}

bool Tiles::SetTilesetTile(Tilemap* pTilemap, s32 tileIndex, const s32& tilesetTileIndex) {
	if (tilesetTileIndex < 0) {
        return false;
    }

    pTilemap->tiles[tileIndex] = tilesetTileIndex;
    return true;
}

bool Tiles::SetTilesetTile(Tilemap* pTilemap, const glm::ivec2& pos, const s32& tilesetTileIndex) {
    if (!PointInMapBounds(pTilemap, pos)) {
        return false;
    }

    const s32 i = GetTileIndex(pTilemap, pos);
	if (i < 0) {
		return false;
	}

    return SetTilesetTile(pTilemap, i, tilesetTileIndex);
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
	fread((void*)&tileset.tiles, sizeof(TilesetTile), TILESET_SIZE, pFile);
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
	fwrite(&tileset.tiles, sizeof(TilesetTile), TILESET_SIZE, pFile);
	fwrite(&tileset.attributes, sizeof(u8), TILESET_ATTRIBUTE_COUNT, pFile);

	fclose(pFile);
}

Tileset* Tiles::GetTileset() {
	return &tileset;
}

#pragma region Compression
bool Tiles::CompressTiles(const u8* tiles, u32 count, std::vector<TileIndexRun>& outCompressed) {
	if (tiles == nullptr) {
		return false;
	}

	outCompressed.clear();
	
	for (u32 i = 0; i < count;) {
		u8 tile = tiles[i];
		u16 length = 1;

		while (i + length < count && tiles[i + length] == tile && length < UCHAR_MAX) {
			length++;
		}

		outCompressed.emplace_back(tile, length);
		i += length;
	}

	return true;
}

bool Tiles::DecompressTiles(const std::vector<TileIndexRun>& compressed, u8* outTiles) {
	if (outTiles == nullptr) {
		return false;
	}

	u32 index = 0;
	for (auto& tileRun : compressed) {
		for (u32 i = 0; i < tileRun.length; i++) {
			outTiles[index++] = tileRun.tile;
		}
	}

	return true;
}
#pragma endregion