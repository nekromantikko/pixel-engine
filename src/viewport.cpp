#include "viewport.h"
#include "level.h"
#include "system.h"
#include "math.h"
#include "tileset.h"

void MoveViewport(Viewport* pViewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel, r32 dx, r32 dy) {
    if (pLevel == nullptr) {
        return;
    }

    const bool verticalScroll = pLevel->flags & Level::LFLAGS_SCROLL_VERTICAL;

    r32 posPrev = verticalScroll ? pViewport->y : pViewport->x;

    pViewport->x += dx;
    pViewport->y += dy;

    r32 xMax = verticalScroll ? NAMETABLE_WIDTH_TILES : pLevel->screenCount * NAMETABLE_WIDTH_TILES;

    if (pViewport->x < 0.0f) {
        pViewport->x = 0.0f;
    }
    else if (pViewport->x + pViewport->w >= xMax) {
        pViewport->x = xMax - pViewport->w;
    }

    r32 yMax = verticalScroll ? pLevel->screenCount * NAMETABLE_HEIGHT_TILES : NAMETABLE_HEIGHT_TILES;

    if (pViewport->y < 0.0f) {
        pViewport->y = 0.0f;
    }
    else if (pViewport->y + pViewport->h >= yMax) {
        pViewport->y = yMax - pViewport->h;
    }

    const s32 previousMetatile = (s32)(posPrev / Tileset::metatileWorldSize);
    const s32 currentMetatile = (s32)((verticalScroll ? pViewport->y : pViewport->x) / Tileset::metatileWorldSize);
    const bool crossedMetatileBoundary = previousMetatile != currentMetatile;
    const r32 scrollDelta = verticalScroll ? dy : dx;
    if (scrollDelta != 0 && crossedMetatileBoundary) {
        static const s32 bufferWidthInMetatiles = 8;
        const u32 viewportDimensionInMetatiles = (s32)(verticalScroll ? pViewport->h : pViewport->w) / Tileset::metatileWorldSize;
        const u32 screenDimensionInMetatiles = verticalScroll ? Level::screenHeightMetatiles : Level::screenWidthMetatiles;

        const s32 metatilesToLoad = abs(currentMetatile - previousMetatile);
        const s32 sign = (s32)Sign(scrollDelta);
        for (s32 b = 0; b < metatilesToLoad; b++) {
            const s32 block = previousMetatile + sign * b;

            const u32 leftMetatileIndex = (u32)(block - bufferWidthInMetatiles);
            const u32 leftScreenIndex = leftMetatileIndex / screenDimensionInMetatiles;
            const u32 leftScreenMetatileOffset = leftMetatileIndex % screenDimensionInMetatiles;
            const u32 leftNametableIndex = leftScreenIndex % NAMETABLE_COUNT;

            const u32 rightMetatileIndex = (u32)(block + bufferWidthInMetatiles + viewportDimensionInMetatiles);
            const u32 rightScreenIndex = rightMetatileIndex / screenDimensionInMetatiles;
            const u32 rightScreenMetatileOffset = rightMetatileIndex % screenDimensionInMetatiles;
            const u32 rightNametableIndex = rightScreenIndex % NAMETABLE_COUNT;

            const u32 scanDimension = verticalScroll ? Level::screenWidthMetatiles : Level::screenHeightMetatiles;

            for (u32 i = 0; i < scanDimension; i++) {
                if (leftScreenIndex < pLevel->screenCount) {
                    const u32 leftOffset = verticalScroll ?
                        i + Level::screenWidthMetatiles * leftScreenMetatileOffset
                        : Level::screenWidthMetatiles * i + leftScreenMetatileOffset;
                    const u8 metatileIndex = pLevel->screens[leftScreenIndex].tiles[leftOffset].metatile;
                    const Vec2 screenTilePos = Level::TileIndexToScreenOffset(leftOffset);
                    Tileset::CopyMetatileToNametable(&pNametable[leftNametableIndex], (u16)screenTilePos.x, (u16)screenTilePos.y, metatileIndex);
                }
                if (rightScreenIndex < pLevel->screenCount) {
                    const u32 rightOffset = verticalScroll ?
                        i + Level::screenWidthMetatiles * rightScreenMetatileOffset
                        : Level::screenWidthMetatiles * i + rightScreenMetatileOffset;
                    const u8 metatileIndex = pLevel->screens[rightScreenIndex].tiles[rightOffset].metatile;
                    const Vec2 screenTilePos = Level::TileIndexToScreenOffset(rightOffset);
                    Tileset::CopyMetatileToNametable(&pNametable[rightNametableIndex], (u16)screenTilePos.x, (u16)screenTilePos.y, metatileIndex);
                }
            }
        }

    }
}

void RefreshViewport(Viewport* viewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel) {
    static const s32 bufferWidthInMetatiles = 8;

    const bool verticalScroll = pLevel->flags & Level::LFLAGS_SCROLL_VERTICAL;

    const s32 dimensionInMetatiles = ((s32)(verticalScroll ? viewport->h : viewport->w) / Tileset::metatileWorldSize) + bufferWidthInMetatiles * 2;
    const s32 startInMetatiles = (s32)((verticalScroll ? viewport->y : viewport->x) / Tileset::metatileWorldSize) - bufferWidthInMetatiles;

    for (int primary = 0; primary < dimensionInMetatiles; primary++) {
        for (int secondary = 0; secondary < (verticalScroll ? Level::screenWidthMetatiles : Level::screenHeightMetatiles); secondary++) {
            const u32 primaryMetatile = startInMetatiles + primary;
            const u32 screenIndex = primaryMetatile / (verticalScroll ? Level::screenHeightMetatiles : Level::screenWidthMetatiles);

            if (screenIndex >= pLevel->screenCount) {
                continue;
            }

            const u32 screenRelativePrimary = primaryMetatile % (verticalScroll ? Level::screenHeightMetatiles : Level::screenWidthMetatiles);
            const u32 screenRelativeSecondary = secondary % (verticalScroll ? Level::screenWidthMetatiles : Level::screenHeightMetatiles);
            const u32 screenTileIndex = verticalScroll
                ? screenRelativePrimary * Level::screenWidthMetatiles + screenRelativeSecondary
                : screenRelativeSecondary * Level::screenWidthMetatiles + screenRelativePrimary;
            const u8 metatileIndex = pLevel->screens[screenIndex].tiles[screenTileIndex].metatile;
            const u32 nametableIndex = screenIndex % NAMETABLE_COUNT;

            const u32 posX = verticalScroll ? screenRelativeSecondary : screenRelativePrimary;
            const u32 posY = verticalScroll ? screenRelativePrimary : screenRelativeSecondary;
            Tileset::CopyMetatileToNametable(&pNametable[nametableIndex], posX * Tileset::metatileWorldSize, posY * Tileset::metatileWorldSize, metatileIndex);
        }
    }
}