#include "game_rendering.h"
#include "game_state.h"
#include "room.h"
#include "dungeon.h"
#include "tiles.h"
#include "rendering_util.h"
#include "game.h"
#include "asset_manager.h"

static constexpr s32 BUFFER_DIM_METATILES = 2;
static constexpr u32 LAYER_SPRITE_COUNT = MAX_SPRITE_COUNT / SPRITE_LAYER_COUNT;

static glm::vec2 viewportPos;
static SpriteLayer spriteLayers[SPRITE_LAYER_COUNT];

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

	const glm::vec2 prevPos = viewportPos;
	viewportPos += delta;

    glm::vec2 playAreaSize = Game::GetCurrentPlayAreaSize();
    const glm::vec2 max = {
        playAreaSize.x - VIEWPORT_WIDTH_METATILES,
        playAreaSize.y - VIEWPORT_HEIGHT_METATILES 
    };

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

    const Tilemap* pTilemap = Game::GetCurrentTilemap();
	if (!pTilemap) {
		return;
	}

	const Tileset* pTileset = AssetManager::GetAsset(pTilemap->tilesetHandle);
    if (!pTileset) {
        return;
    }

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            // Only load tiles that weren't already loaded
            if (xStartPrev <= x && x <= xEndPrev && yStartPrev <= y && y <= yEndPrev) {
                continue;
            }

            const s32 tilesetIndex = pTilemap->GetTilesetTileIndex({ x, y });
            

            if (tilesetIndex < 0) {
                continue;
            }

			const TilesetTile& tile = pTileset->tiles[tilesetIndex];

            const s32 nametableIndex = Rendering::Util::GetNametableIndexFromMetatilePos({ x, y });
            const glm::ivec2 nametableOffset = Rendering::Util::GetNametableOffsetFromMetatilePos({ x, y });

            const Metatile& metatile = tile.metatile;
            Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset, metatile);
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
    Sprite sprite = pMetasprite->GetSprites()[spriteIndex];

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

    sprite.x += pos.x;
    sprite.y += pos.y;

    return sprite;
}

#pragma region Public API
void Game::Rendering::Init() {
	viewportPos = { 0, 0 };
    ClearSpriteLayers(true);

    // Init chr memory
	// TODO: Figure out what to do with these hardcoded values
    constexpr ChrBankHandle asciiBankHandle(8539419541591404705);
    constexpr ChrBankHandle mapBankHandle(12884965207213169338);
    constexpr ChrBankHandle debugTilesetBankHandle(5051829589002943406);
    constexpr ChrBankHandle fgBankHandle(1554696323931700844);
	CopyBankTiles(asciiBankHandle, 0, 0, 0, CHR_SIZE_TILES);
	CopyBankTiles(mapBankHandle, 0, 1, 0, CHR_SIZE_TILES);
    CopyBankTiles(debugTilesetBankHandle, 0, 2, 0, CHR_SIZE_TILES);
    CopyBankTiles(fgBankHandle, 0, 4, 0, CHR_SIZE_TILES);

    constexpr PaletteHandle debug0PaletteHandle(9826404639351995940);
    constexpr PaletteHandle debug1PaletteHandle(15753953292764895790);
    constexpr PaletteHandle worldMapPaletteHandle(235843673484981221);
    constexpr PaletteHandle dungeonMapPaletteHandle(17785843363754367893);
    constexpr PaletteHandle freyaPaletteHandle(12681477220579246228);
    constexpr PaletteHandle freyaDarkPaletteHandle(5947198864976396277);
    constexpr PaletteHandle checkpointPaletteHandle(17417799732251940800);
    constexpr PaletteHandle goldBluePaletteHandle(16493657319985968026);
	constexpr PaletteHandle greenRedPaletteHandle(14621729332936982450);

	CopyPaletteColors(debug0PaletteHandle, 0);
	CopyPaletteColors(worldMapPaletteHandle, 1);
	CopyPaletteColors(debug1PaletteHandle, 2);
	CopyPaletteColors(dungeonMapPaletteHandle, 3);
    CopyPaletteColors(goldBluePaletteHandle, 8);
	CopyPaletteColors(freyaPaletteHandle, 9);
	CopyPaletteColors(freyaDarkPaletteHandle, 10);
	CopyPaletteColors(greenRedPaletteHandle, 11);
	CopyPaletteColors(checkpointPaletteHandle, 12);
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

    const s32 xStart = viewportPos.x - BUFFER_DIM_METATILES;
    const s32 xEnd = VIEWPORT_WIDTH_METATILES + viewportPos.x + BUFFER_DIM_METATILES;

    const s32 yStart = viewportPos.y - BUFFER_DIM_METATILES;
    const s32 yEnd = VIEWPORT_HEIGHT_METATILES + viewportPos.y + BUFFER_DIM_METATILES;

    const Tilemap* pTilemap = Game::GetCurrentTilemap();
	if (!pTilemap) {
		return;
	}


	const Tileset* pTileset = AssetManager::GetAsset(pTilemap->tilesetHandle);
    if (!pTileset) {
        return;
    }

    for (s32 x = xStart; x < xEnd; x++) {
        for (s32 y = yStart; y < yEnd; y++) {
            const s32 tilesetIndex = pTilemap->GetTilesetTileIndex({ x, y });

            if (tilesetIndex < 0) {
                continue;
            }

			const TilesetTile& tile = pTileset->tiles[tilesetIndex];

            const s32 nametableIndex = ::Rendering::Util::GetNametableIndexFromMetatilePos({ x, y });
            const glm::ivec2 nametableOffset = ::Rendering::Util::GetNametableOffsetFromMetatilePos({ x, y });

            const Metatile& metatile = tile.metatile;
            ::Rendering::Util::SetNametableMetatile(&pNametables[nametableIndex], nametableOffset, metatile);
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

bool Game::Rendering::DrawMetaspriteSprite(u8 layerIndex, MetaspriteHandle metaspriteHandle, u32 spriteIndex, glm::i16vec2 pos, bool hFlip, bool vFlip, s32 paletteOverride) {
    const Metasprite* pMetasprite = AssetManager::GetAsset(metaspriteHandle);
    if (!pMetasprite) {
        return false;
    }

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

bool Game::Rendering::DrawMetasprite(u8 layerIndex, MetaspriteHandle metaspriteHandle, glm::i16vec2 pos, bool hFlip, bool vFlip, s32 paletteOverride) {
    const Metasprite* pMetasprite = AssetManager::GetAsset(metaspriteHandle);
    if (!pMetasprite) {
        return false;
    }

    Sprite* outSprites = GetNextFreeSprite(layerIndex, pMetasprite->spriteCount);
	if (outSprites == nullptr) {
		return false;
	}

    for (u32 i = 0; i < pMetasprite->spriteCount; i++) {
		outSprites[i] = TransformMetaspriteSprite(pMetasprite, i, pos, hFlip, vFlip, paletteOverride);
    }

    return true;
}

void Game::Rendering::CopyBankTiles(ChrBankHandle bankHandle, u32 bankOffset, u32 sheetIndex, u32 sheetOffset, u32 count) {
	ChrSheet* pBank = AssetManager::GetAsset(bankHandle);
	if (!pBank) {
		DEBUG_ERROR("Failed to load bank %llu\n", bankHandle);
		return;
	}

	const ChrTile* pBankTiles = pBank->tiles + bankOffset;
	ChrTile* pSheetTiles = ::Rendering::GetChrPtr(sheetIndex)->tiles + sheetOffset;

    memcpy(pSheetTiles, pBankTiles, sizeof(ChrTile) * count);
}

bool Game::Rendering::GetPalettePresetColors(PaletteHandle paletteHandle, u8* pOutColors) {
    Palette* pPalette = AssetManager::GetAsset(paletteHandle);
    if (!pPalette) {
        DEBUG_ERROR("Failed to load palette %llu\n", paletteHandle);
        return false;
    }

	memcpy(pOutColors, pPalette->colors, PALETTE_COLOR_COUNT);
	return true;
}

void Game::Rendering::WritePaletteColors(u8 paletteIndex, u8* pColors) {
	memcpy(::Rendering::GetPalettePtr(paletteIndex)->colors, pColors, PALETTE_COLOR_COUNT);
}

void Game::Rendering::CopyPaletteColors(PaletteHandle paletteHandle, u8 paletteIndex) {
	Palette* pPalette = AssetManager::GetAsset(paletteHandle);
	if (!pPalette) {
		DEBUG_ERROR("Failed to load palette %llu\n", paletteHandle);
		return;
	}

	memcpy(::Rendering::GetPalettePtr(paletteIndex)->colors, pPalette->colors, PALETTE_COLOR_COUNT);
}

#ifdef EDITOR
u32 Assets::GetMetaspriteSize(const Metasprite* pHeader) {
	u32 result = sizeof(Metasprite);
	if (pHeader) {
		result += pHeader->spriteCount * sizeof(Sprite);
	}
	return result;
}
#endif

#pragma endregion