#include "viewport.h"
#include "level.h"
#include "system.h"

void MoveViewport(Viewport *viewport, Rendering::RenderContext* pRenderContext, Level* pLevel, f32 dx, f32 dy) {
    f32 xPrevious = viewport->x;
    viewport->x += dx;
    if (viewport->x < 0.0f) {
        viewport->x = 0.0f;
    }
    else if (viewport->x + viewport->w >= pLevel->screenCount * NAMETABLE_WIDTH_TILES * TILE_SIZE) {
        viewport->x = (pLevel->screenCount * NAMETABLE_WIDTH_TILES * TILE_SIZE) - viewport->w;
    }

    viewport->y += dy;
    if (viewport->y < 0.0f) {
        viewport->y = 0.0f;
    }
    else if (viewport->y + viewport->h >= NAMETABLE_HEIGHT_TILES * TILE_SIZE) {
        viewport->y = (NAMETABLE_HEIGHT_TILES * TILE_SIZE) - viewport->h;
    }

    // TODO: Update nametables when scrolling
    bool crossedBlockBoundary = ((s32)xPrevious % (TILE_SIZE * 2)) != ((s32)viewport->x % (TILE_SIZE * 2));
    float bufferWidth = 128.0f;
    if (dx != 0 && crossedBlockBoundary) {
        u32 leftBlockIndex = (u32)((viewport->x - bufferWidth) / (TILE_SIZE * 2));
        u32 leftScreenIndex = leftBlockIndex / (NAMETABLE_WIDTH_TILES / 2);
        u32 leftScreenBlockOffset = leftBlockIndex % (NAMETABLE_WIDTH_TILES / 2);
        u32 leftScreenTileOffset = leftScreenBlockOffset * 2;
        u32 rightBlockIndex = (u32)((viewport->x + bufferWidth + viewport->w) / (TILE_SIZE * 2));
        u32 rightScreenIndex = rightBlockIndex / (NAMETABLE_WIDTH_TILES / 2);
        u32 rightScreenBlockOffset = rightBlockIndex % (NAMETABLE_WIDTH_TILES / 2);
        u32 rightScreenTileOffset = rightScreenBlockOffset * 2;

        for (int i = 0; i < NAMETABLE_HEIGHT_TILES; i++) {
            if (leftScreenIndex < pLevel->screenCount) {
                u32 leftOffset = NAMETABLE_WIDTH_TILES * i + leftScreenTileOffset;
                u32 xBlock = leftScreenTileOffset / 4;
                u32 yBlock = i / 4;
                u32 attributeBlockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
                Rendering::WriteNametable(pRenderContext, leftScreenIndex % NAMETABLE_COUNT, 2, leftOffset, &pLevel->screens[leftScreenIndex].tiles[leftOffset]);
                Rendering::WriteNametable(pRenderContext, leftScreenIndex % NAMETABLE_COUNT, 1, NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex, &pLevel->screens[leftScreenIndex].tiles[NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex]);
            }
            if (rightScreenIndex < pLevel->screenCount) {
                u32 rightOffset = NAMETABLE_WIDTH_TILES * i + rightScreenTileOffset;
                u32 xBlock = rightScreenTileOffset / 4;
                u32 yBlock = i / 4;
                u32 attributeBlockIndex = (NAMETABLE_WIDTH_TILES / 4) * yBlock + xBlock;
                Rendering::WriteNametable(pRenderContext, rightScreenIndex % NAMETABLE_COUNT, 2, rightOffset, &pLevel->screens[rightScreenIndex].tiles[rightOffset]);
                Rendering::WriteNametable(pRenderContext, rightScreenIndex % NAMETABLE_COUNT, 1, NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex, &pLevel->screens[rightScreenIndex].tiles[NAMETABLE_ATTRIBUTE_OFFSET + attributeBlockIndex]);
            }
        }
    }
}