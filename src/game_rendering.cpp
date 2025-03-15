#include "game_rendering.h"
#include "game_state.h"
#include "level.h"
#include "dungeon.h"
#include "system.h"
#include "tiles.h"
#include "metasprite.h"
#include "rendering_util.h"
#include "game.h"

static constexpr s32 BUFFER_DIM_METATILES = 2;
static constexpr u32 LAYER_SPRITE_COUNT = MAX_SPRITE_COUNT / SPRITE_LAYER_COUNT;

static glm::vec2 viewportPos;
static SpriteLayer spriteLayers[SPRITE_LAYER_COUNT];

static u8 paletteColors[PALETTE_MEMORY_SIZE];

static ChrSheet chrBanks[3];

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
    const Tilemap* pTilemap = Game::GetCurrentRoomTemplate()->pTilemap;

	if (pTilemap == nullptr) {
		return;
	}

	const glm::vec2 prevPos = viewportPos;
	viewportPos += delta;

	const glm::vec2 max = { 
        (pTilemap->width - 1) * VIEWPORT_WIDTH_METATILES,
        (pTilemap->height - 1) * VIEWPORT_HEIGHT_METATILES };

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

            const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, { x, y });
            const TilesetTile* tile = Tiles::GetTilesetTile(pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = Rendering::Util::GetNametableIndexFromMetatilePos({ x, y });
            const glm::ivec2 nametableOffset = Rendering::Util::GetNametableOffsetFromMetatilePos({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
            Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset, metatile, palette);
        }
    }
}

static void ClearSprites(Sprite* spr, u32 count) {
    for (int i = 0; i < count; i++) {
        Sprite& sprite = spr[i];

        // Really just moves the sprites off screen (This is how the NES clears sprites as well)
        sprite.y = 288;
    }
}

static Sprite TransformMetaspriteSprite(const Metasprite* pMetasprite, u32 spriteIndex, glm::i16vec2 pos, bool hFlip, bool vFlip, s32 paletteOverride) {
    Sprite sprite = pMetasprite->spritesRelativePos[spriteIndex];

    // Apply horizontal flip
    if (hFlip) {
        sprite.flipHorizontal = !sprite.flipHorizontal;
        sprite.x = -sprite.x - TILE_DIM_PIXELS;
    }

    // Apply vertical flip
    if (vFlip) {
        sprite.flipVertical = !sprite.flipVertical;
        sprite.y = -sprite.y - TILE_DIM_PIXELS;
    }

    // Override palette if specified
    if (paletteOverride != -1) {
        sprite.palette = paletteOverride;
    }

    // Check for wraparound
    const s16 extendedX = pos.x + ::Rendering::Util::SignExtendSpritePos(sprite.x);
    const s16 extendedY = pos.y + ::Rendering::Util::SignExtendSpritePos(sprite.y);
    
    if (extendedX > VIEWPORT_WIDTH_PIXELS || extendedY > VIEWPORT_HEIGHT_PIXELS) {
        sprite.x = 0;
        sprite.y = 288;
    } else {
        sprite.x += pos.x; // Moved to else block for clarity
        sprite.y += pos.y; // Moved to else block for clarity
    }

    return sprite;
}

#pragma region Public API
void Game::Rendering::Init() {
	viewportPos = { 0, 0 };
    ClearSpriteLayers(true);

    // Init chr memory
    // TODO: Pre-process these instead of loading from bitmap at runtime!
    ::Rendering::Util::CreateChrSheet("assets/chr000.bmp", &chrBanks[0]);
	CopyBankTiles(0, 0, 0, 0, CHR_SIZE_TILES);
    ::Rendering::Util::CreateChrSheet("assets/chr001.bmp", &chrBanks[1]);
	CopyBankTiles(1, 0, 1, 0, CHR_SIZE_TILES);

    // Player bank
    ::Rendering::Util::CreateChrSheet("assets/player.bmp", &chrBanks[2]);


    ::Rendering::Util::LoadPaletteColorsFromFile("assets/palette.dat", paletteColors);
    memcpy(::Rendering::GetPalettePtr(0), paletteColors, PALETTE_MEMORY_SIZE);
}

glm::vec2 Game::Rendering::GetViewportPos() {
    return viewportPos;
}
// Returns the new position of the viewport
glm::vec2 Game::Rendering::SetViewportPos(const glm::vec2& pos, bool loadTiles) {
    const glm::vec2 delta = pos - viewportPos;
    MoveViewport(delta, loadTiles);
	UpdateScreenScroll();
	return viewportPos;
}

void Game::Rendering::RefreshViewport() {
    Nametable* pNametables = ::Rendering::GetNametablePtr(0);
    const Tilemap* pTilemap = Game::GetCurrentRoomTemplate()->pTilemap;

    if (pTilemap == nullptr) {
        return;
    }

    const s32 xStart = viewportPos.x - BUFFER_DIM_METATILES;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + viewportPos.x + BUFFER_DIM_METATILES;

    const s32 yStart = viewportPos.y - BUFFER_DIM_METATILES;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + viewportPos.y + BUFFER_DIM_METATILES;

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, { x, y });
            const TilesetTile* tile = Tiles::GetTilesetTile(pTilemap, tilesetIndex);

            if (!tile) {
                continue;
            }

            const s32 nametableIndex = ::Rendering::Util::GetNametableIndexFromMetatilePos({ x, y });
            const glm::ivec2 nametableOffset = ::Rendering::Util::GetNametableOffsetFromMetatilePos({ x, y });

            const Metatile& metatile = tile->metatile;
            const s32 palette = Tiles::GetTilesetPalette(pTilemap->pTileset, tilesetIndex);
            ::Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset, metatile, palette);
        }
    }
}

bool Game::Rendering::PositionInViewportBounds(const glm::vec2& pos) {
    return pos.x >= viewportPos.x &&
        pos.x < viewportPos.x + VIEWPORT_WIDTH_METATILES &&
        pos.y >= viewportPos.y &&
        pos.y < viewportPos.y + VIEWPORT_HEIGHT_METATILES;
}

glm::i16vec2 Game::Rendering::WorldPosToScreenPixels(const glm::vec2& pos) {
    return glm::i16vec2{
        (s16)glm::roundEven((pos.x - viewportPos.x) * METATILE_DIM_PIXELS),
        (s16)glm::roundEven((pos.y - viewportPos.y) * METATILE_DIM_PIXELS)
    };
}

void Game::Rendering::ClearSpriteLayers(bool fullClear) {
    const Sprite* pSprites = ::Rendering::GetSpritesPtr(0);

    for (u32 i = 0; i < SPRITE_LAYER_COUNT; i++) {
        SpriteLayer& layer = spriteLayers[i];

        u32 beginIndex = i << 10;
        Sprite* pBeginSprite = ::Rendering::GetSpritesPtr(beginIndex);

        const u32 spritesToClear = fullClear ? LAYER_SPRITE_COUNT : layer.spriteCount;
        ClearSprites(pBeginSprite, spritesToClear);
        layer.pNextSprite = pBeginSprite;
        layer.spriteCount = 0;
    }
}

Sprite* Game::Rendering::GetNextFreeSprite(u8 layerIndex, u32 count) {
	if (layerIndex >= SPRITE_LAYER_COUNT) {
		return nullptr;
	}

	SpriteLayer& layer = spriteLayers[layerIndex];

    if (layer.spriteCount + count > LAYER_SPRITE_COUNT) {
        return nullptr;
    }

    Sprite* result = layer.pNextSprite;
    layer.spriteCount += count;
    layer.pNextSprite += count;

    return result;
}

bool Game::Rendering::DrawSprite(u8 layerIndex, const Sprite& sprite) {
    Sprite* outSprite = GetNextFreeSprite(layerIndex);
    if (outSprite == nullptr) {
        return false;
    }
    *outSprite = sprite;

    return true;
}

bool Game::Rendering::DrawMetaspriteSprite(u8 layerIndex, u32 metaspriteIndex, u32 spriteIndex, glm::i16vec2 pos, bool hFlip, bool vFlip, s32 paletteOverride) {
    const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);
	if (spriteIndex >= pMetasprite->spriteCount) {
		return false;
	}

    Sprite* outSprite = GetNextFreeSprite(layerIndex);
    if (outSprite == nullptr) {
        return false;
    }

	*outSprite = TransformMetaspriteSprite(pMetasprite, spriteIndex, pos, hFlip, vFlip, paletteOverride);

	return true;
}

bool Game::Rendering::DrawMetasprite(u8 layerIndex, u32 metaspriteIndex, glm::i16vec2 pos, bool hFlip, bool vFlip, s32 paletteOverride) {
    const Metasprite* pMetasprite = Metasprites::GetMetasprite(metaspriteIndex);
    Sprite* outSprites = GetNextFreeSprite(layerIndex, pMetasprite->spriteCount);
	if (outSprites == nullptr) {
		return false;
	}

    for (u32 i = 0; i < pMetasprite->spriteCount; i++) {
		outSprites[i] = TransformMetaspriteSprite(pMetasprite, i, pos, hFlip, vFlip, paletteOverride);
    }

    return true;
}

void Game::Rendering::CopyBankTiles(u32 bankIndex, u32 bankOffset, u32 sheetIndex, u32 sheetOffset, u32 count) {
	const ChrTile* pBankTiles = chrBanks[bankIndex].tiles + bankOffset;
	ChrTile* pSheetTiles = ::Rendering::GetChrPtr(sheetIndex)->tiles + sheetOffset;

    memcpy(pSheetTiles, pBankTiles, sizeof(ChrTile) * count);
}

// TODO: Actually implement palette presets
void Game::Rendering::GetPalettePresetColors(u8 presetIndex, u8* pOutColors) {
	if (presetIndex >= PALETTE_COUNT) {
		return;
	}

	memcpy(pOutColors, paletteColors + presetIndex * PALETTE_COLOR_COUNT, PALETTE_COLOR_COUNT);
}

void Game::Rendering::WritePaletteColors(u8 paletteIndex, u8* pColors) {
	memcpy(::Rendering::GetPalettePtr(paletteIndex)->colors, pColors, PALETTE_COLOR_COUNT);
}
#pragma endregion