#include "viewport.h"
#include "level.h"
#include "system.h"
#include "math.h"
#include "tileset.h"

void MoveViewport(Viewport *viewport, Rendering::RenderContext* pRenderContext, const Level* const pLevel, r32 dx, r32 dy) {
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
            s32 block = previousMetatile + sign * b;

            u32 leftMetatileIndex = (u32)(block - bufferWidthInMetatiles);
            u32 leftScreenIndex = leftMetatileIndex / LEVEL_SCREEN_WIDTH_METATILES;
            u32 leftScreenMetatileOffset = leftMetatileIndex % LEVEL_SCREEN_WIDTH_METATILES;

            u32 rightMetatileIndex = (u32)(block + bufferWidthInMetatiles + viewportWidthInMetatiles);
            u32 rightScreenIndex = rightMetatileIndex / LEVEL_SCREEN_WIDTH_METATILES;
            u32 rightScreenMetatileOffset = rightMetatileIndex % LEVEL_SCREEN_WIDTH_METATILES;

            for (int i = 0; i < LEVEL_SCREEN_HEIGHT_METATILES; i++) {
                if (leftScreenIndex < pLevel->screenCount) {
                    u32 leftOffset = LEVEL_SCREEN_WIDTH_METATILES * i + leftScreenMetatileOffset;
                    Tileset::WriteMetatileToNametable(pRenderContext, leftScreenIndex, leftScreenMetatileOffset * Tileset::metatileWorldSize, i * Tileset::metatileWorldSize, pLevel->screens[leftScreenIndex].metatiles[leftOffset]);
                }
                if (rightScreenIndex < pLevel->screenCount) {
                    u32 rightOffset = LEVEL_SCREEN_WIDTH_METATILES * i + rightScreenMetatileOffset;
                    Tileset::WriteMetatileToNametable(pRenderContext, rightScreenIndex, rightScreenMetatileOffset * Tileset::metatileWorldSize, i * Tileset::metatileWorldSize, pLevel->screens[rightScreenIndex].metatiles[rightOffset]);
                }
            }
        }

    }
}

void RefreshViewport(Viewport* viewport, Rendering::RenderContext* pRenderContext, const Level* const pLevel) {
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

            u32 screenRelativeX = xMetatile % LEVEL_SCREEN_WIDTH_METATILES;
            u32 screenRelativeY = y % LEVEL_SCREEN_HEIGHT_METATILES;
            u32 screenMetatileIndex = screenRelativeY * LEVEL_SCREEN_WIDTH_METATILES + screenRelativeX;

            Tileset::WriteMetatileToNametable(pRenderContext, screenIndex, screenRelativeX * Tileset::metatileWorldSize, screenRelativeY * Tileset::metatileWorldSize, pLevel->screens[screenIndex].metatiles[screenMetatileIndex]);
        }
    }
}