#include "viewport.h"
#include "level.h"
#include "system.h"
#include "math.h"
#include "tileset.h"

void MoveViewport(Viewport *viewport, Rendering::Nametable* pNametable, const Level* const pLevel, r32 dx, r32 dy) {
    r32 xPrevious = viewport->x;
    viewport->x += dx;
    if (viewport->x < 0.0f) {
        viewport->x = 0.0f;
    }
    else if (viewport->x + viewport->w >= pLevel->screenCount * NAMETABLE_WIDTH_TILES) {
        viewport->x = (pLevel->screenCount * NAMETABLE_WIDTH_TILES) - viewport->w;
    }

    viewport->y += dy;
    if (viewport->y < 0.0f) {
        viewport->y = 0.0f;
    }
    else if (viewport->y + viewport->h >= NAMETABLE_HEIGHT_TILES) {
        viewport->y = (NAMETABLE_HEIGHT_TILES) - viewport->h;
    }

    const s32 previousMetatile = (s32)(xPrevious / Tileset::metatileWorldSize);
    const s32 currentMetatile = (s32)(viewport->x / Tileset::metatileWorldSize);
    bool crossedMetatileBoundary = previousMetatile != currentMetatile;
    if (dx != 0 && crossedMetatileBoundary) {
        static const s32 bufferWidthInMetatiles = 8;
        const s32 viewportWidthInMetatiles = (s32)viewport->w / Tileset::metatileWorldSize;

        s32 metatilesToLoad = abs(currentMetatile - previousMetatile);
        s32 sign = (s32)Sign(dx);
        for (s32 b = 0; b < metatilesToLoad; b++) {
            const s32 block = previousMetatile + sign * b;

            const u32 leftMetatileIndex = (u32)(block - bufferWidthInMetatiles);
            const u32 leftScreenIndex = leftMetatileIndex / LEVEL_SCREEN_WIDTH_METATILES;
            const u32 leftScreenMetatileOffset = leftMetatileIndex % LEVEL_SCREEN_WIDTH_METATILES;
            const u32 leftNametableIndex = leftScreenIndex % NAMETABLE_COUNT;

            const u32 rightMetatileIndex = (u32)(block + bufferWidthInMetatiles + viewportWidthInMetatiles);
            const u32 rightScreenIndex = rightMetatileIndex / LEVEL_SCREEN_WIDTH_METATILES;
            const u32 rightScreenMetatileOffset = rightMetatileIndex % LEVEL_SCREEN_WIDTH_METATILES;
            const u32 rightNametableIndex = rightScreenIndex % NAMETABLE_COUNT;

            for (int i = 0; i < LEVEL_SCREEN_HEIGHT_METATILES; i++) {
                if (leftScreenIndex < pLevel->screenCount) {
                    const u32 leftOffset = LEVEL_SCREEN_WIDTH_METATILES * i + leftScreenMetatileOffset;
                    const u8 metatileIndex = pLevel->screens[leftScreenIndex].tiles[leftOffset].metatile;
                    Tileset::CopyMetatileToNametable(&pNametable[leftNametableIndex], leftScreenMetatileOffset * Tileset::metatileWorldSize, i * Tileset::metatileWorldSize, metatileIndex);
                }
                if (rightScreenIndex < pLevel->screenCount) {
                    const u32 rightOffset = LEVEL_SCREEN_WIDTH_METATILES * i + rightScreenMetatileOffset;
                    const u8 metatileIndex = pLevel->screens[rightScreenIndex].tiles[rightOffset].metatile;
                    Tileset::CopyMetatileToNametable(&pNametable[rightNametableIndex], rightScreenMetatileOffset * Tileset::metatileWorldSize, i * Tileset::metatileWorldSize, metatileIndex);
                }
            }
        }

    }
}

void RefreshViewport(Viewport* viewport, Rendering::Nametable* pNametable, const Level* const pLevel) {
    static const s32 bufferWidthInMetatiles = 8;
    const s32 widthInMetatiles = ((s32)viewport->w / Tileset::metatileWorldSize) + bufferWidthInMetatiles * 2;
    const s32 xInMetatiles = (s32)(viewport->x / Tileset::metatileWorldSize);
    const s32 xStart = xInMetatiles - bufferWidthInMetatiles;

    for (int x = 0; x < widthInMetatiles; x++) {
        for (int y = 0; y < LEVEL_SCREEN_HEIGHT_METATILES; y++) {
            u32 xMetatile = xStart + x;
            u32 screenIndex = xMetatile / LEVEL_SCREEN_WIDTH_METATILES;

            if (screenIndex >= pLevel->screenCount) {
                continue;
            }

            const u32 screenRelativeX = xMetatile % LEVEL_SCREEN_WIDTH_METATILES;
            const u32 screenRelativeY = y % LEVEL_SCREEN_HEIGHT_METATILES;
            const u32 screenTileIndex = screenRelativeY * LEVEL_SCREEN_WIDTH_METATILES + screenRelativeX;
            const u8 metatileIndex = pLevel->screens[screenIndex].tiles[screenTileIndex].metatile;
            const u32 nametableIndex = screenIndex % NAMETABLE_COUNT;
            Tileset::CopyMetatileToNametable(&pNametable[nametableIndex], screenRelativeX * Tileset::metatileWorldSize, screenRelativeY * Tileset::metatileWorldSize, metatileIndex);
        }
    }
}