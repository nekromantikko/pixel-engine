#include "viewport.h"
#include "level.h"
#include "system.h"
#include "math.h"
#include "tileset.h"

static constexpr s32 bufferDimMetatiles = 2;

void MoveViewport(Viewport* pViewport, Nametable* pNametable, const Level::Level* pLevel, r32 dx, r32 dy, bool loadTiles) {
    if (pLevel == nullptr) {
        return;
    }

    const r32 prevX = pViewport->x;
    const r32 prevY = pViewport->y;

    pViewport->x += dx;
    pViewport->y += dy;

    r32 xMax = (pLevel->width - 1) * Level::screenWidthTiles;

    if (pViewport->x < 0.0f) {
        pViewport->x = 0.0f;
    }
    else if (pViewport->x >= xMax) {
        pViewport->x = xMax;
    }

    r32 yMax = (pLevel->height - 1) * Level::screenHeightTiles;

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
    const s32 prevMetatileX = (s32)(prevX / METATILE_DIM_TILES);
    const s32 prevMetatileY = (s32)(prevY / METATILE_DIM_TILES);

    const s32 currMetatileX = (s32)(pViewport->x / METATILE_DIM_TILES);
    const s32 currMetatileY = (s32)(pViewport->y / METATILE_DIM_TILES);

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
        const u32 screenOffsetX = x % Level::screenWidthMetatiles;
        const u32 nametableOffsetX = x % NAMETABLE_WIDTH_METATILES;

        for (s32 y = yStart; y < yEnd; y++) {
            if (!Level::TileInLevelBounds(pLevel, { x, y })) {
                continue;
            }

            // Only load tiles that weren't already loaded
            if (xStartPrev <= x && x <= xEndPrev && yStartPrev <= y && y <= yEndPrev) {
                continue;
            }

            const u32 screenIndex = Level::TilemapToScreenIndex(pLevel, { x, y });
            const u32 screenOffsetY = y % Level::screenHeightMetatiles;

            const u32 nametableIndex = Level::TilemapToNametableIndex({ x, y });
            const u32 nametableOffsetY = y % NAMETABLE_HEIGHT_METATILES;

            const u32 screenMetatileIndex = Level::TilemapToMetatileIndex({ x,y });
            const u8 tilesetIndex = pLevel->screens[screenIndex].tiles[screenMetatileIndex].metatile;
            Tileset::CopyMetatileToNametable(&pNametable[nametableIndex], (u16)nametableOffsetX * METATILE_DIM_TILES, (u16)nametableOffsetY * METATILE_DIM_TILES, tilesetIndex);
        }
    }
}

void RefreshViewport(Viewport* pViewport, Nametable* pNametable, const Level::Level* pLevel) {
    if (pLevel == nullptr) {
        return;
    }

    const s32 xInMetatiles = (s32)(pViewport->x / METATILE_DIM_TILES);
    const s32 yInMetatiles = (s32)(pViewport->y / METATILE_DIM_TILES);

    const s32 xStart = xInMetatiles - bufferDimMetatiles;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + xInMetatiles + bufferDimMetatiles;

    const s32 yStart = yInMetatiles - bufferDimMetatiles;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + yInMetatiles + bufferDimMetatiles;

    for (s32 x = xStart; x < xEnd; x++) {
        const u32 screenOffsetX = x % Level::screenWidthMetatiles;
        const u32 nametableOffsetX = x % NAMETABLE_WIDTH_METATILES;

        for (s32 y = yStart; y < yEnd; y++) {
            if (!Level::TileInLevelBounds(pLevel, { x, y })) {
                continue;
            }

            const u32 screenIndex = Level::TilemapToScreenIndex(pLevel, { x, y });
            const u32 screenOffsetY = y % Level::screenHeightMetatiles;

            const u32 nametableIndex = Level::TilemapToNametableIndex({ x, y });
            const u32 nametableOffsetY = y % NAMETABLE_HEIGHT_METATILES;

            const u32 screenMetatileIndex = Level::TilemapToMetatileIndex({ x,y });
            const u8 tilesetIndex = pLevel->screens[screenIndex].tiles[screenMetatileIndex].metatile;
            Tileset::CopyMetatileToNametable(&pNametable[nametableIndex], (u16)nametableOffsetX * METATILE_DIM_TILES, (u16)nametableOffsetY * METATILE_DIM_TILES, tilesetIndex);
        }
    }
}