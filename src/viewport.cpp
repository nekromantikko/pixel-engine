#include "viewport.h"
#include "level.h"
#include "system.h"
#include "math.h"
#include "tileset.h"

static constexpr s32 bufferWidthInMetatiles = 8;

void MoveViewport(Viewport* pViewport, Rendering::Nametable* pNametable, const Level::Level* const pLevel, r32 dx, r32 dy, bool loadTiles) {
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
    else if (pViewport->x + VIEWPORT_WIDTH_TILES >= xMax) {
        pViewport->x = xMax - VIEWPORT_WIDTH_TILES;
    }

    r32 yMax = verticalScroll ? pLevel->screenCount * NAMETABLE_HEIGHT_TILES : NAMETABLE_HEIGHT_TILES;

    if (pViewport->y < 0.0f) {
        pViewport->y = 0.0f;
    }
    else if (pViewport->y + VIEWPORT_HEIGHT_TILES >= yMax) {
        pViewport->y = yMax - VIEWPORT_HEIGHT_TILES;
    }
    
    if (!loadTiles) {
        return;
    }

    const s32 previousMetatile = (s32)(posPrev / Tileset::metatileWorldSize);
    const s32 currentMetatile = (s32)((verticalScroll ? pViewport->y : pViewport->x) / Tileset::metatileWorldSize);
    const bool crossedMetatileBoundary = previousMetatile != currentMetatile;
    const r32 scrollDelta = verticalScroll ? dy : dx;
    if (scrollDelta != 0 && crossedMetatileBoundary) {
        const u32 viewportDimensionInMetatiles = (s32)(verticalScroll ? VIEWPORT_HEIGHT_TILES : VIEWPORT_WIDTH_TILES) / Tileset::metatileWorldSize;
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
    if (pLevel == nullptr) {
        return;
    }

    const bool verticalScroll = pLevel->flags & Level::LFLAGS_SCROLL_VERTICAL;

    // Calculate the region to refresh in metatiles
    const s32 startX = verticalScroll ? 0 : ((s32)viewport->x / Tileset::metatileWorldSize) - bufferWidthInMetatiles;
    const s32 startY = verticalScroll ? ((s32)viewport->y / Tileset::metatileWorldSize) - bufferWidthInMetatiles : 0;
    const s32 widthInMetatiles = verticalScroll ? Level::screenWidthMetatiles : (VIEWPORT_WIDTH_TILES / Tileset::metatileWorldSize) + bufferWidthInMetatiles * 2;
    const s32 heightInMetatiles = verticalScroll ? (VIEWPORT_HEIGHT_TILES / Tileset::metatileWorldSize) + bufferWidthInMetatiles * 2 : Level::screenHeightMetatiles;

    // Refresh each metatile in the region
    for (s32 y = 0; y < heightInMetatiles; y++) {
        const s32 metatileY = startY + y;

        for (s32 x = 0; x < widthInMetatiles; x++) {
            const s32 metatileX = startX + x;

            const u32 screenIndex = Level::TilemapToScreenIndex(pLevel, { metatileX, metatileY });
            if (screenIndex >= pLevel->screenCount) {
                continue;
            }

            const u32 screenTileIndex = Level::TilemapToMetatileIndex({ metatileX, metatileY });
            const u8 metatileIndex = pLevel->screens[screenIndex].tiles[screenTileIndex].metatile;
            const Vec2 screenTilePos = Level::TileIndexToScreenOffset(screenTileIndex);

            Tileset::CopyMetatileToNametable(&pNametable[screenIndex % NAMETABLE_COUNT], (u16)screenTilePos.x, (u16)screenTilePos.y, metatileIndex);
        }
    }
}

u8 GetMetatileAtNametablePosition(const Level::Level* const pLevel, const Viewport* pViewport, u32 nametableIndex, u32 tileX, u32 tileY) {
    if (pLevel == nullptr) {
        return 0;
    }

    const bool verticalScroll = pLevel->flags & Level::LFLAGS_SCROLL_VERTICAL;

    // Convert from tile coordinates to metatile coordinates
    const u32 metatileX = tileX / Tileset::metatileWorldSize;
    const u32 metatileY = tileY / Tileset::metatileWorldSize;

    // Get the viewport position in metatiles
    const s32 viewportMetatilePos = (s32)((verticalScroll ? pViewport->y : pViewport->x) / Tileset::metatileWorldSize);

    // Calculate which screen this nametable represents based on viewport position
    const u32 viewportScreen = viewportMetatilePos / (verticalScroll ? Level::screenHeightMetatiles : Level::screenWidthMetatiles);
    const u32 screenForNametable = viewportScreen - (viewportScreen % NAMETABLE_COUNT) + nametableIndex;

    if (screenForNametable >= pLevel->screenCount) {
        return 0;
    }

    // Convert nametable-local coordinates to screen-local coordinates
    u32 screenTileIndex;
    if (verticalScroll) {
        screenTileIndex = metatileY * Level::screenWidthMetatiles + metatileX;
    }
    else {
        screenTileIndex = metatileY * Level::screenWidthMetatiles + metatileX;
    }

    return pLevel->screens[screenForNametable].tiles[screenTileIndex].metatile;
}