#include "viewport.h"
#include "level.h"
#include "system.h"
#include "tiles.h"
#include "rendering_util.h"

static constexpr s32 bufferDimMetatiles = 2;

void MoveViewport(Viewport* pViewport, Nametable* pNametable, const Tilemap *pTilemap, r32 dx, r32 dy, bool loadTiles) {
    if (pTilemap == nullptr) {
        return;
    }

    const r32 prevX = pViewport->x;
    const r32 prevY = pViewport->y;

    pViewport->x += dx;
    pViewport->y += dy;

    r32 xMax = (pTilemap->width - 1) * VIEWPORT_WIDTH_METATILES;

    if (pViewport->x < 0.0f) {
        pViewport->x = 0.0f;
    }
    else if (pViewport->x >= xMax) {
        pViewport->x = xMax;
    }

    r32 yMax = (pTilemap->height - 1) * VIEWPORT_HEIGHT_METATILES;

    if (pViewport->y < 0.0f) {
        pViewport->y = 0.0f;
    }
    else if (pViewport->y >= yMax) {
        pViewport->y = yMax;
    }
    
    if (!loadTiles) {
        return;
    }

    // These are the metatiles the top left corner of the viewport is in, before and after
    const s32 prevMetatileX = (s32)glm::floor(prevX);
    const s32 prevMetatileY = (s32)glm::floor(prevY);

    const s32 currMetatileX = (s32)glm::floor(pViewport->x);
    const s32 currMetatileY = (s32)glm::floor(pViewport->y);

    // If no new metatiles need loading, early return
    if (currMetatileX == prevMetatileX && currMetatileY == prevMetatileY) {
        return;
    }

    const s32 xStartPrev = prevMetatileX;
    const s32 xEndPrev = VIEWPORT_WIDTH_METATILES + prevMetatileX;
    const s32 xStart = currMetatileX - bufferDimMetatiles;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + currMetatileX + bufferDimMetatiles;

    const s32 yStartPrev = prevMetatileY;
    const s32 yEndPrev = VIEWPORT_HEIGHT_METATILES + prevMetatileY;
    const s32 yStart = currMetatileY - bufferDimMetatiles;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + currMetatileY + bufferDimMetatiles;

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            // Only load tiles that weren't already loaded
            if (xStartPrev <= x && x <= xEndPrev && yStartPrev <= y && y <= yEndPrev) {
                continue;
            }

            const s32 tilesetIndex = Tiles::GetTilesetIndex(pTilemap, { x, y });
            const MapTile* tile = Tiles::GetMapTile(pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = Tiles::GetNametableIndex({ x, y });
            const glm::ivec2 nametableOffset = Tiles::GetNametableOffset({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
            Rendering::Util::SetNametableMetatile(&pNametable[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
        }
    }
}

void RefreshViewport(Viewport* pViewport, Nametable* pNametable, const Tilemap* pTilemap) {
    if (pTilemap == nullptr) {
        return;
    }

    const s32 xStart = pViewport->x - bufferDimMetatiles;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + pViewport->x + bufferDimMetatiles;

    const s32 yStart = pViewport->y - bufferDimMetatiles;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + pViewport->y + bufferDimMetatiles;

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            const s32 tilesetIndex = Tiles::GetTilesetIndex(pTilemap, { x, y });
            const MapTile* tile = Tiles::GetMapTile(pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = Tiles::GetNametableIndex({ x, y });
            const glm::ivec2 nametableOffset = Tiles::GetNametableOffset({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
            Rendering::Util::SetNametableMetatile(&pNametable[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
        }
    }
}