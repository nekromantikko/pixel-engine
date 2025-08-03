#include "tilemap.h"

bool Tiles::PointInBounds(const Tilemap* pTilemap, const glm::vec2& pos) {
	if (pos.x < 0 || pos.y < 0) {
		return false;
	}

	if (pos.x >= pTilemap->width || pos.y >= pTilemap->height) {
		return false;
	}

	return true;
}

s32 Tiles::GetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos) {
	if (!PointInBounds(pTilemap, pos)) {
		return -1;
	}

    return (pos.x % pTilemap->width) + (pos.y % pTilemap->height) * pTilemap->width;
}

s32 Tiles::GetTilesetTileIndex(const Tilemap* pTilemap, const glm::ivec2& pos) {
    if (!PointInBounds(pTilemap, pos)) {
        return -1;
    }

    const s32 i = GetTileIndex(pTilemap, pos);
	if (i < 0) {
		return -1;
	}

	return pTilemap->GetTileData()[i];
}

bool Tiles::SetTilesetTile(const Tilemap* pTilemap, s32 tileIndex, const s32& tilesetTileIndex) {
	if (tilesetTileIndex < 0) {
        return false;
    }

	pTilemap->GetTileData()[tileIndex] = tilesetTileIndex;
    return true;
}

bool Tiles::SetTilesetTile(const Tilemap* pTilemap, const glm::ivec2& pos, const s32& tilesetTileIndex) {
    if (!PointInBounds(pTilemap, pos)) {
        return false;
    }

    const s32 i = GetTileIndex(pTilemap, pos);
	if (i < 0) {
		return false;
	}

    return SetTilesetTile(pTilemap, i, tilesetTileIndex);
}