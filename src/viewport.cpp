#include "viewport.h"
#include "level.h"
#include "system.h"
#include "tiles.h"
#include "rendering_util.h"

static constexpr s32 bufferDimMetatiles = 2;

glm::vec2 Game::MoveViewport(const glm::vec2& viewportPos, Nametable* pNametables, const Tilemap* pTilemap, const glm::vec2& delta, bool loadTiles) {
    if (pTilemap == nullptr) {
        return viewportPos;
    }

	const glm::vec2 prevPos = viewportPos;
	glm::vec2 newPos = viewportPos + delta;

	const glm::vec2 max = { 
        (pTilemap->width - 1) * VIEWPORT_WIDTH_METATILES, 
        (pTilemap->height - 1) * VIEWPORT_HEIGHT_METATILES };

	newPos = glm::clamp(newPos, glm::vec2(0), max);
    
    if (!loadTiles) {
        return newPos;
    }

    // These are the metatiles the top left corner of the viewport is in, before and after
    const glm::ivec2 prevMetatile = glm::floor(prevPos);
	const glm::ivec2 currMetatile = glm::floor(newPos);

    // If no new metatiles need loading, early return
    if (prevMetatile == currMetatile) {
        return newPos;
    }

    const s32 xStartPrev = prevMetatile.x;
    const s32 xEndPrev = VIEWPORT_WIDTH_METATILES + prevMetatile.x;
    const s32 xStart = currMetatile.x - bufferDimMetatiles;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + currMetatile.x + bufferDimMetatiles;

    const s32 yStartPrev = prevMetatile.y;
    const s32 yEndPrev = VIEWPORT_HEIGHT_METATILES + prevMetatile.y;
    const s32 yStart = currMetatile.y - bufferDimMetatiles;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + currMetatile.y + bufferDimMetatiles;

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            // Only load tiles that weren't already loaded
            if (xStartPrev <= x && x <= xEndPrev && yStartPrev <= y && y <= yEndPrev) {
                continue;
            }

            const s32 tilesetIndex = Tiles::GetTilesetIndex(pTilemap, { x, y });
            const TilesetTile* tile = Tiles::GetTilesetTile(pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = Tiles::GetNametableIndex({ x, y });
            const glm::ivec2 nametableOffset = Tiles::GetNametableOffset({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
            Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
        }
    }

    return newPos;
}

void Game::RefreshViewport(const glm::vec2& viewportPos, Nametable* pNametables, const Tilemap* pTilemap) {
    if (pTilemap == nullptr) {
        return;
    }

    const s32 xStart = viewportPos.x - bufferDimMetatiles;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + viewportPos.x + bufferDimMetatiles;

    const s32 yStart = viewportPos.y - bufferDimMetatiles;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + viewportPos.y + bufferDimMetatiles;

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            const s32 tilesetIndex = Tiles::GetTilesetIndex(pTilemap, { x, y });
            const TilesetTile* tile = Tiles::GetTilesetTile(pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = Tiles::GetNametableIndex({ x, y });
            const glm::ivec2 nametableOffset = Tiles::GetNametableOffset({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
            Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
        }
    }
}