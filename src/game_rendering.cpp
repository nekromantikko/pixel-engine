#include "game_rendering.h"
#include "rendering.h"
#include "game_state.h"
#include "tilemap.h"
#include "rendering_util.h"
#include "game.h"
#include "asset_manager.h"
#include "memory_arena.h"

#pragma region Virtual CHR
constexpr u32 MAX_VIRTUAL_BANKS = 128;
constexpr u32 VIRTUAL_CHR_BANK_SIZE = MAX_VIRTUAL_BANKS * CHR_SIZE_TILES;
constexpr u32 PHYSICAL_TILE_COUNT = CHR_COUNT * CHR_SIZE_TILES;
constexpr u32 PHYSICAL_TILE_BITS = PHYSICAL_TILE_COUNT >> 3;
constexpr u32 FG_PAGE_BITS_OFFSET = (CHR_PAGE_COUNT * CHR_SIZE_TILES) >> 3;
static constexpr u32 MAX_ACTIVE_BANKS = 8;

enum TileDrawType {
    TILE_DRAW_TYPE_BG,
	TILE_DRAW_TYPE_FG,
};

struct PhysicalCHRBank {
    s16 tiles[CHR_SIZE_TILES];
};

struct BankAddressMapping {
    u64 bankId;
    u16 virtualIndex; // The start of a block
};

BankAddressMapping g_bankAddressMapping[MAX_VIRTUAL_BANKS];

s16 g_physicalToVirtualChrMap[PHYSICAL_TILE_COUNT];
u8 g_physicalTileUsageFlags[PHYSICAL_TILE_BITS] = { 0 }; // Bitmask for physical tile usage
// Hierarchical mapping of virtual banks to physical banks
PhysicalCHRBank* g_virtualToPhysicalBankMap[MAX_VIRTUAL_BANKS];

u8 g_activePageCount = 0;
PhysicalCHRBank g_activePageTables[MAX_ACTIVE_BANKS];

static s16 GetPhysicalTileIndex(s16 virtualTileIndex) {
    if (virtualTileIndex < 0) {
        return -1;
    }

    u8 virtualBankIndex = u8(virtualTileIndex >> 8);
    u8 tileOffset = u8(virtualTileIndex & 0xFF);

    PhysicalCHRBank* pTable = g_virtualToPhysicalBankMap[virtualBankIndex];
    if (pTable == nullptr) {
        return -1;
    }

    return pTable->tiles[tileOffset];
}

static s16 GetVirtualTileIndex(ChrBankHandle bankHandle, u8 tileIndex) {
    u64 bankId = bankHandle.id;
    for (u32 i = 0; i < MAX_VIRTUAL_BANKS; i++) {
        if (g_bankAddressMapping[i].bankId == bankId) {
            u16 virtualIndex = g_bankAddressMapping[i].virtualIndex;
            return virtualIndex + tileIndex;
        }
    }
    return -1;
}

static void CopyBankTiles(ChrBankHandle bankHandle, u32 bankOffset, u32 sheetIndex, u32 sheetOffset, u32 count) {
    ChrSheet* pBank = AssetManager::GetAsset(bankHandle);
    if (!pBank) {
        DEBUG_ERROR("Failed to load bank %llu\n", bankHandle);
        return;
    }

    const ChrTile* pBankTiles = pBank->tiles + bankOffset;
    ChrTile* pSheetTiles = ::Rendering::GetChrPtr(sheetIndex)->tiles + sheetOffset;

    memcpy(pSheetTiles, pBankTiles, sizeof(ChrTile) * count);
}

static void MarkPhysicalTileUsed(s16 physicalTileIndex) {
	g_physicalTileUsageFlags[physicalTileIndex >> 3] |= (1 << (physicalTileIndex & 0x07));
}

static s16 GetFirstUnusedPhysicalTile(TileDrawType type) {
	u32 startIndex = (type == TILE_DRAW_TYPE_BG) ? 0 : FG_PAGE_BITS_OFFSET;
	for (u32 i = startIndex; i < PHYSICAL_TILE_BITS; i++) {
		if (g_physicalTileUsageFlags[i] != 0xFF) {
			// Find the first unused bit in this byte
			for (u32 j = 0; j < 8; j++) {
				if (!(g_physicalTileUsageFlags[i] & (1 << j))) {
					return (i << 3) + j;
				}
			}
		}
	}
	return -1; // No unused tiles found
}

static void ClearPhysicalTileUsageFlags(TileDrawType type) {
	u32 offset = (type == TILE_DRAW_TYPE_BG) ? 0 : FG_PAGE_BITS_OFFSET;
	memset(g_physicalTileUsageFlags + offset, 0, sizeof(g_physicalTileUsageFlags) / 2);
}

static s16 RequestTileForDrawing(ChrBankHandle bankHandle, u8 tileIndex, TileDrawType type) {
    s16 virtualTileIndex = GetVirtualTileIndex(bankHandle, tileIndex);
	if (virtualTileIndex < 0) {
		// This shouldn't happen, but for some reason the tile is not in the virtual mapping
		DEBUG_ERROR("Virtual tile index for bank %llu, tile %u is invalid\n", bankHandle.id, tileIndex);
		return -1;
	}

	s16 physicalTileIndex = GetPhysicalTileIndex(virtualTileIndex);
    if (physicalTileIndex >= 0) {
		// Tile already exists in physical memory, return it
		MarkPhysicalTileUsed(physicalTileIndex);
		return physicalTileIndex;
	}

	// Create mapping for the virtual tile
    u8 virtualBankIndex = u8(virtualTileIndex >> 8);
	u8 tileOffset = u8(virtualTileIndex & 0xFF);

	PhysicalCHRBank* pTable = g_virtualToPhysicalBankMap[virtualBankIndex];
	if (pTable == nullptr) {
		// If the bank is not active, we need to allocate a new one
		if (g_activePageCount >= MAX_ACTIVE_BANKS) {
			DEBUG_ERROR("No more active CHR banks available\n");
			return -1;
		}
		pTable = &g_activePageTables[g_activePageCount++];
		g_virtualToPhysicalBankMap[virtualBankIndex] = pTable;
	}

	// Assign the tile to the physical bank
	physicalTileIndex = GetFirstUnusedPhysicalTile(type);
    if (physicalTileIndex < 0) {
		DEBUG_ERROR("No unused physical tiles available for bank %llu, tile %u\n", bankHandle.id, tileIndex);
		return -1;
    }
	pTable->tiles[tileOffset] = physicalTileIndex;

	// Update the mapping
	// First, clear the previous mapping if it exists
	s16 previousVirtualTileIndex = g_physicalToVirtualChrMap[physicalTileIndex];
	if (previousVirtualTileIndex >= 0) {
		// If the tile was already mapped, we need to clear it
		u8 previousVirtualBankIndex = u8(previousVirtualTileIndex >> 8);
		u8 previousTileOffset = u8(previousVirtualTileIndex & 0xFF);
		g_virtualToPhysicalBankMap[previousVirtualBankIndex]->tiles[previousTileOffset] = -1;
		DEBUG_LOG("Cleared previous mapping for physical tile %d from virtual bank %u, tile %u\n", physicalTileIndex, previousVirtualBankIndex, previousTileOffset);
	}
	g_physicalToVirtualChrMap[physicalTileIndex] = virtualTileIndex;
	// Copy the tile data from the virtual bank to the physical bank
    u32 chrPage = (physicalTileIndex >> 8) & 7;
    u32 chrTileIndex = physicalTileIndex & 0xFF;
	CopyBankTiles(bankHandle, tileIndex, chrPage, chrTileIndex, 1);

	MarkPhysicalTileUsed(physicalTileIndex);
	return physicalTileIndex;
}

static void InitVirtualCHR() {
    size_t chrBankCount;
    AssetManager::GetAllAssetInfosByType(ASSET_TYPE_CHR_BANK, chrBankCount, nullptr);
    ArenaMarker scratchMarker = ArenaAllocator::GetMarker(ARENA_SCRATCH);
    const AssetEntry** ppChrEntries = ArenaAllocator::PushArray<const AssetEntry*>(ARENA_SCRATCH, chrBankCount);
    AssetManager::GetAllAssetInfosByType(ASSET_TYPE_CHR_BANK, chrBankCount, ppChrEntries);

    for (size_t i = 0; i < chrBankCount; i++) {
        g_bankAddressMapping[i].bankId = ppChrEntries[i]->id;
        g_bankAddressMapping[i].virtualIndex = i << 8; // Each virtual bank can hold 256 tiles, so shift by 8 bits
        DEBUG_LOG("Virtual CHR bank %zu: %s, Virtual Index = %u\n", i, ppChrEntries[i]->relativePath, g_bankAddressMapping[i].virtualIndex);
    }

    for (u32 i = 0; i < CHR_COUNT * CHR_SIZE_TILES; i++) {
        g_physicalToVirtualChrMap[i] = -1;
    }

    for (u32 i = 0; i < MAX_VIRTUAL_BANKS; i++) {
        g_virtualToPhysicalBankMap[i] = nullptr;
    }

    for (u32 i = 0; i < MAX_ACTIVE_BANKS; i++) {
        for (u32 t = 0; t < CHR_SIZE_TILES; t++) {
            g_activePageTables[i].tiles[t] = -1;
        }
    }

    ArenaAllocator::PopToMarker(ARENA_SCRATCH, scratchMarker);
}
#pragma endregion

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

            const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, { x, y });
            

            if (tilesetIndex < 0) {
                continue;
            }

			const TilesetTile& tile = pTileset->tiles[tilesetIndex];
			Game::Rendering::DrawBackgroundMetatile(pTileset->chrBankHandle, tile.metatile, { x, y });
        }
    }
}

static void ClearSprites(Sprite* spr, u32 count) {
    for (u32 i = 0; i < count; i++) {
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
	InitVirtualCHR();

	const PaletteHandle debug0PaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/debug_0.dat");
	const PaletteHandle debug1PaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/debug_1.dat");
	const PaletteHandle worldMapPaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/world_map.dat");
	const PaletteHandle dungeonMapPaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/dungeon_map.dat");
	const PaletteHandle freyaPaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/freya.dat");
	const PaletteHandle freyaDarkPaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/freya_dark.dat");
	const PaletteHandle checkpointPaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/checkpoint.dat");
	const PaletteHandle goldBluePaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/gold_blue.dat");
	const PaletteHandle greenRedPaletteHandle = AssetManager::GetAssetHandle<PaletteHandle>("palettes/green_red.dat");

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
            const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, { x, y });

            if (tilesetIndex < 0) {
                continue;
            }

            const TilesetTile& tile = pTileset->tiles[tilesetIndex];
            Game::Rendering::DrawBackgroundMetatile(pTileset->chrBankHandle, tile.metatile, { x, y });
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

    // Clear fg tile usage
	ClearPhysicalTileUsageFlags(TILE_DRAW_TYPE_FG);
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

bool Game::Rendering::DrawSprite(u8 layerIndex, ChrBankHandle bankHandle, const Sprite& sprite) {
    Sprite* outSprite = GetNextFreeSprite(layerIndex);
    if (outSprite == nullptr) {
        return false;
    }
    *outSprite = sprite;
    outSprite->tileId = RequestTileForDrawing(bankHandle, sprite.tileId, TILE_DRAW_TYPE_FG);

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
    outSprite->tileId = RequestTileForDrawing(pMetasprite->chrBankHandle, outSprite->tileId, TILE_DRAW_TYPE_FG);

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
		outSprites[i].tileId = RequestTileForDrawing(pMetasprite->chrBankHandle, outSprites[i].tileId, TILE_DRAW_TYPE_FG);
    }

    return true;
}

void Game::Rendering::DrawBackgroundTile(ChrBankHandle bankHandle, const BgTile& tile, const glm::ivec2& pos) {
	s16 physicalTileIndex = RequestTileForDrawing(bankHandle, tile.tileId, TILE_DRAW_TYPE_BG);
	if (physicalTileIndex < 0) {
		DEBUG_ERROR("Failed to request tile %u from bank %llu\n", tile.tileId, bankHandle.id);
		return;
	}

	BgTile physicalTile = tile;
    physicalTile.tileId = physicalTileIndex;

    const u32 nametableIndex = ::Rendering::Util::GetNametableIndexFromTilePos(pos);
	Nametable* pNametable = ::Rendering::GetNametablePtr(nametableIndex);
    const glm::ivec2 nametableOffset = ::Rendering::Util::GetNametableOffsetFromTilePos(pos);
	::Rendering::Util::SetNametableTile(pNametable, nametableOffset, physicalTile);
}

void Game::Rendering::DrawBackgroundMetatile(ChrBankHandle bankHandle, const Metatile& metatile, const glm::ivec2& pos) {
	Metatile physicalMetatile = metatile;
	for (u32 i = 0; i < METATILE_TILE_COUNT; i++) {
		s16 physicalTileIndex = RequestTileForDrawing(bankHandle, metatile.tiles[i].tileId, TILE_DRAW_TYPE_BG);
		if (physicalTileIndex < 0) {
			DEBUG_ERROR("Failed to request tile %u from bank %llu\n", metatile.tiles[i].tileId, bankHandle.id);
			return;
		}
		physicalMetatile.tiles[i].tileId = physicalTileIndex;
	}

	const u32 nametableIndex = ::Rendering::Util::GetNametableIndexFromMetatilePos(pos);
	Nametable* pNametable = ::Rendering::GetNametablePtr(nametableIndex);
	const glm::ivec2 nametableOffset = ::Rendering::Util::GetNametableOffsetFromMetatilePos(pos);

	::Rendering::Util::SetNametableMetatile(pNametable, nametableOffset, physicalMetatile);
}

void Game::Rendering::ClearBackgroundTile(const glm::ivec2& pos) {
	static constexpr BgTile emptyTile = {};
    const u32 nametableIndex = ::Rendering::Util::GetNametableIndexFromTilePos(pos);
    Nametable* pNametable = ::Rendering::GetNametablePtr(nametableIndex);
    const glm::ivec2 nametableOffset = ::Rendering::Util::GetNametableOffsetFromTilePos(pos);
    ::Rendering::Util::SetNametableTile(pNametable, nametableOffset, emptyTile);
}

void Game::Rendering::ClearBackgroundMetatile(const glm::ivec2& pos) {
	Nametable* pNametables = ::Rendering::GetNametablePtr(0);
	const u32 nametableIndex = ::Rendering::Util::GetNametableIndexFromMetatilePos(pos);
	const glm::ivec2 nametableOffset = ::Rendering::Util::GetNametableOffsetFromMetatilePos(pos);
	const u32 firstTileIndex = ::Rendering::Util::GetNametableTileIndexFromMetatileOffset(nametableOffset);

    static constexpr BgTile emptyTile = {};
	pNametables[nametableIndex].tiles[firstTileIndex] = emptyTile;
	pNametables[nametableIndex].tiles[firstTileIndex + 1] = emptyTile;
	pNametables[nametableIndex].tiles[firstTileIndex + NAMETABLE_DIM_TILES] = emptyTile;
	pNametables[nametableIndex].tiles[firstTileIndex + NAMETABLE_DIM_TILES + 1] = emptyTile;
}

void Game::Rendering::CopyLevelTileToNametable(const Tilemap* pTilemap, const glm::ivec2& worldPos) {
    const Tileset* pTileset = AssetManager::GetAsset(pTilemap->tilesetHandle);

    const s32 tilesetIndex = Tiles::GetTilesetTileIndex(pTilemap, worldPos);
    const TilesetTile& tile = pTileset->tiles[tilesetIndex];

    const Metatile& metatile = tile.metatile;
	DrawBackgroundMetatile(pTileset->chrBankHandle, metatile, worldPos);
}

void Game::Rendering::FlushBackgroundTiles() {
    // Clear nametables
	Nametable* pNametables = ::Rendering::GetNametablePtr(0);
	memset(pNametables, 0, sizeof(Nametable) * NAMETABLE_COUNT);
	// Clear physical tile usage flags
	ClearPhysicalTileUsageFlags(TILE_DRAW_TYPE_BG);
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
#pragma endregion