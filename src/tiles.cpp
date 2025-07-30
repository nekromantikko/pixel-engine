#include "tiles.h"
#include "rendering_util.h"
#include "debug.h"
#include <limits>
#include "asset_manager.h"

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

	return pTilemap->GetTileData()[i];
}

const TilesetTile* Tiles::GetTilesetTile(const Tilemap* pTilemap, const s32& tilesetTileIndex) {
    if (tilesetTileIndex < 0) {
        return nullptr;
    }

	const Tileset* pTileset = Assets::GetTilemapTileset(pTilemap);
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

	pTilemap->GetTileData()[tileIndex] = tilesetTileIndex;
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

Tileset* Assets::GetTilemapTileset(const Tilemap* pTilemap) {
	if (!pTilemap) {
		return nullptr;
	}

	return AssetManager::GetAsset(pTilemap->tilesetHandle);
}
