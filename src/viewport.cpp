#include "viewport.h"
#include "level.h"
#include "system.h"
#include "tiles.h"
#include "rendering_util.h"
#include "game.h"

static constexpr s32 BUFFER_DIM_METATILES = 2;

static glm::vec2 viewportPos;

static void UpdateScreenScroll() {
    Scanline* pScanlines = Rendering::GetScanlinePtr(0);

    // Drugs mode
    /*for (int i = 0; i < 288; i++) {
        float sine = glm::sin(gameplayFramesElapsed / 60.f + (i / 16.0f));
        const Scanline state = {
            (s32)((viewport.x + sine / 4) * METATILE_DIM_PIXELS),
            (s32)(viewport.y * METATILE_DIM_PIXELS)
        };
        pScanlines[i] = state;
    }*/

    const Scanline state = {
        (s32)(viewportPos.x * METATILE_DIM_PIXELS),
        (s32)(viewportPos.y * METATILE_DIM_PIXELS)
    };
    for (int i = 0; i < SCANLINE_COUNT; i++) {
        pScanlines[i] = state;
    }
}

static void MoveViewport(const glm::vec2& delta, bool loadTiles) {
    Nametable* pNametables = Rendering::GetNametablePtr(0);
    const Level* pLevel = Game::GetCurrentLevel();

	if (pLevel == nullptr || pLevel->pTilemap == nullptr) {
		return;
	}

	const glm::vec2 prevPos = viewportPos;
	viewportPos += delta;

	const glm::vec2 max = { 
        (pLevel->pTilemap->width - 1) * VIEWPORT_WIDTH_METATILES,
        (pLevel->pTilemap->height - 1) * VIEWPORT_HEIGHT_METATILES };

    viewportPos = glm::clamp(viewportPos, glm::vec2(0), max);
    
    if (!loadTiles) {
        return;
    }

    // These are the metatiles the top left corner of the viewport is in, before and after
    const glm::ivec2 prevMetatile = glm::floor(prevPos);
	const glm::ivec2 currMetatile = glm::floor(viewportPos);

    // If no new metatiles need loading, early return
    if (prevMetatile == currMetatile) {
        return;
    }

    const s32 xStartPrev = prevMetatile.x;
    const s32 xEndPrev = VIEWPORT_WIDTH_METATILES + prevMetatile.x;
    const s32 xStart = currMetatile.x - BUFFER_DIM_METATILES;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + currMetatile.x + BUFFER_DIM_METATILES;

    const s32 yStartPrev = prevMetatile.y;
    const s32 yEndPrev = VIEWPORT_HEIGHT_METATILES + prevMetatile.y;
    const s32 yStart = currMetatile.y - BUFFER_DIM_METATILES;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + currMetatile.y + BUFFER_DIM_METATILES;

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            // Only load tiles that weren't already loaded
            if (xStartPrev <= x && x <= xEndPrev && yStartPrev <= y && y <= yEndPrev) {
                continue;
            }

            const s32 tilesetIndex = Tiles::GetTilesetIndex(pLevel->pTilemap, { x, y });
            const TilesetTile* tile = Tiles::GetTilesetTile(pLevel->pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = Tiles::GetNametableIndex({ x, y });
            const glm::ivec2 nametableOffset = Tiles::GetNametableOffset({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pLevel->pTilemap->pTileset, tilesetIndex);
            Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
        }
    }
}

#pragma region Public API
glm::vec2 Game::GetViewportPos() {
    return viewportPos;
}
// Returns the new position of the viewport
glm::vec2 Game::SetViewportPos(const glm::vec2& pos, bool loadTiles) {
    const glm::vec2 delta = pos - viewportPos;
    MoveViewport(delta, loadTiles);
	UpdateScreenScroll();
	return viewportPos;
}

void Game::RefreshViewport() {
    Nametable* pNametables = Rendering::GetNametablePtr(0);
    const Level* pLevel = Game::GetCurrentLevel();

    if (pLevel == nullptr || pLevel->pTilemap == nullptr) {
        return;
    }

    const s32 xStart = viewportPos.x - BUFFER_DIM_METATILES;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + viewportPos.x + BUFFER_DIM_METATILES;

    const s32 yStart = viewportPos.y - BUFFER_DIM_METATILES;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + viewportPos.y + BUFFER_DIM_METATILES;

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            const s32 tilesetIndex = Tiles::GetTilesetIndex(pLevel->pTilemap, { x, y });
            const TilesetTile* tile = Tiles::GetTilesetTile(pLevel->pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = Tiles::GetNametableIndex({ x, y });
            const glm::ivec2 nametableOffset = Tiles::GetNametableOffset({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pLevel->pTilemap->pTileset, tilesetIndex);
            Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset.x, nametableOffset.y, metatile, palette);
        }
    }
}

bool Game::PositionInViewportBounds(const glm::vec2& pos) {
    return pos.x >= viewportPos.x &&
        pos.x < viewportPos.x + VIEWPORT_WIDTH_METATILES &&
        pos.y >= viewportPos.y &&
        pos.y < viewportPos.y + VIEWPORT_HEIGHT_METATILES;
}

glm::i16vec2 Game::WorldPosToScreenPixels(const glm::vec2& pos) {
    return glm::i16vec2{
        (s16)glm::roundEven((pos.x - viewportPos.x) * METATILE_DIM_PIXELS),
        (s16)glm::roundEven((pos.y - viewportPos.y) * METATILE_DIM_PIXELS)
    };
}
#pragma endregion