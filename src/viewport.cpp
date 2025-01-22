#include "viewport.h"
#include "level.h"
#include "system.h"
#include "math.h"
#include "tileset.h"

static constexpr s32 bufferDimMetatiles = 2;

void MoveViewport(Viewport* pViewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel, r32 dx, r32 dy, bool loadTiles) {
    if (pLevel == nullptr) {
        return;
    }

    const bool verticalScroll = pLevel->flags & Level::LFLAGS_SCROLL_VERTICAL;

    const r32 prevX = pViewport->x;
    const r32 prevY = pViewport->y;

    pViewport->x += dx;
    pViewport->y += dy;

    r32 xMax = verticalScroll ? Level::screenWidthTiles : pLevel->screenCount * Level::screenWidthTiles;

    if (pViewport->x < 0.0f) {
        pViewport->x = 0.0f;
    }
    else if (pViewport->x + VIEWPORT_WIDTH_TILES >= xMax) {
        pViewport->x = xMax - VIEWPORT_WIDTH_TILES;
    }

    r32 yMax = verticalScroll ? pLevel->screenCount * Level::screenHeightTiles : Level::screenHeightTiles;

    if (pViewport->y < 0.0f) {
        pViewport->y = 0.0f;
    }
    else if (pViewport->y + VIEWPORT_HEIGHT_TILES >= yMax) {
        pViewport->y = yMax - VIEWPORT_HEIGHT_TILES;
    }
    
    if (!loadTiles) {
        return;
    }

    // These are the metatiles the top left corner of the viewport is in, before and after
    const s32 prevMetatileX = (s32)(prevX / Tileset::metatileWorldSize);
    const s32 prevMetatileY = (s32)(prevY / Tileset::metatileWorldSize);

    const s32 currMetatileX = (s32)(pViewport->x / Tileset::metatileWorldSize);
    const s32 currMetatileY = (s32)(pViewport->y / Tileset::metatileWorldSize);

    // If no new metatiles need loading, early return
    if (currMetatileX == prevMetatileX && currMetatileY == prevMetatileY) {
        return;
    }

    const s32 xStartPrev = prevMetatileX;
    const s32 xEndPrev = Level::viewportWidthMetatiles + prevMetatileX;
    const s32 xStart = currMetatileX - bufferDimMetatiles;
    const s32 xEnd = Level::viewportWidthMetatiles + currMetatileX + bufferDimMetatiles;

    const s32 yStartPrev = prevMetatileY;
    const s32 yEndPrev = Level::viewportHeightMetatiles + prevMetatileY;
    const s32 yStart = currMetatileY - bufferDimMetatiles;
    const s32 yEnd = Level::viewportHeightMetatiles + currMetatileY + bufferDimMetatiles;

    for (s32 x = xStart; x < xEnd; x++) {
        const u32 screenOffsetX = x % Level::screenWidthMetatiles;
        const u32 nametableOffsetX = x % Level::nametableWidthMetatiles;

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
            const u32 nametableOffsetY = y % Level::nametableHeightMetatiles;

            const u32 screenMetatileIndex = Level::TilemapToMetatileIndex({ x,y });
            const u8 tilesetIndex = pLevel->screens[screenIndex].tiles[screenMetatileIndex].metatile;
            Tileset::CopyMetatileToNametable(&pNametable[nametableIndex], (u16)nametableOffsetX * Tileset::metatileWorldSize, (u16)nametableOffsetY * Tileset::metatileWorldSize, tilesetIndex);
        }
    }
}

void RefreshViewport(Viewport* pViewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel) {
    if (pLevel == nullptr) {
        return;
    }

    const s32 xInMetatiles = (s32)(pViewport->x / Tileset::metatileWorldSize);
    const s32 yInMetatiles = (s32)(pViewport->y / Tileset::metatileWorldSize);

    const s32 xStart = xInMetatiles - bufferDimMetatiles;
    const s32 xEnd = Level::viewportWidthMetatiles + xInMetatiles + bufferDimMetatiles;

    const s32 yStart = yInMetatiles - bufferDimMetatiles;
    const s32 yEnd = Level::viewportHeightMetatiles + yInMetatiles + bufferDimMetatiles;

    for (s32 x = xStart; x < xEnd; x++) {
        const u32 screenOffsetX = x % Level::screenWidthMetatiles;
        const u32 nametableOffsetX = x % Level::nametableWidthMetatiles;

        for (s32 y = yStart; y < yEnd; y++) {
            if (!Level::TileInLevelBounds(pLevel, { x, y })) {
                continue;
            }

            const u32 screenIndex = Level::TilemapToScreenIndex(pLevel, { x, y });
            const u32 screenOffsetY = y % Level::screenHeightMetatiles;

            const u32 nametableIndex = Level::TilemapToNametableIndex({ x, y });
            const u32 nametableOffsetY = y % Level::nametableHeightMetatiles;

            const u32 screenMetatileIndex = Level::TilemapToMetatileIndex({ x,y });
            const u8 tilesetIndex = pLevel->screens[screenIndex].tiles[screenMetatileIndex].metatile;
            Tileset::CopyMetatileToNametable(&pNametable[nametableIndex], (u16)nametableOffsetX * Tileset::metatileWorldSize, (u16)nametableOffsetY * Tileset::metatileWorldSize, tilesetIndex);
        }
    }
}