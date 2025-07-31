#include "tiles.h"

bool Tilemap::PointInBounds(const glm::vec2& pos) const {
	if (pos.x < 0 || pos.y < 0) {
		return false;
	}

	if (pos.x >= width || pos.y >= height) {
		return false;
	}

	return true;
}

s32 Tilemap::GetTileIndex(const glm::ivec2& pos) const {
	if (!PointInBounds(pos)) {
		return -1;
	}

    return (pos.x % width) + (pos.y % height) * width;
}

s32 Tilemap::GetTilesetTileIndex(const glm::ivec2& pos) const {
    if (!PointInBounds(pos)) {
        return -1;
    }

    const s32 i = GetTileIndex(pos);
	if (i < 0) {
		return -1;
	}

	return GetTileData()[i];
}

bool Tilemap::SetTilesetTile(s32 tileIndex, const s32& tilesetTileIndex) const {
	if (tilesetTileIndex < 0) {
        return false;
    }

	GetTileData()[tileIndex] = tilesetTileIndex;
    return true;
}

bool Tilemap::SetTilesetTile(const glm::ivec2& pos, const s32& tilesetTileIndex) const {
    if (!PointInBounds(pos)) {
        return false;
    }

    const s32 i = GetTileIndex(pos);
	if (i < 0) {
		return false;
	}

    return SetTilesetTile(i, tilesetTileIndex);
}