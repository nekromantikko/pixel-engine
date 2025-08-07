#include "virtual_chr_manager.h"
#include "asset_manager.h"
#include "game_rendering.h"
#include "rendering.h"
#include "debug.h"
#include <cstring>

struct ChrSlot {
    ChrBankHandle bankHandle;
    u32 bankOffset;
    u32 accessCount;
    u32 lastAccessFrame;
    bool isManuallyManaged;
    bool isInUse;
};

static ChrSlot chrSlots[CHR_COUNT];
static u32 frameCounter = 0;

// Reserved CHR sheets for manual management (first 4 sheets)
constexpr u32 MANUAL_CHR_SHEET_COUNT = 4;
constexpr u32 VIRTUAL_CHR_SHEET_START = MANUAL_CHR_SHEET_COUNT;

void VirtualChrManager::Init() {
    // Initialize all slots as unused
    for (u32 i = 0; i < CHR_COUNT; i++) {
        chrSlots[i] = {
            .bankHandle = ChrBankHandle::Null(),
            .bankOffset = 0,
            .accessCount = 0,
            .lastAccessFrame = 0,
            .isManuallyManaged = i < MANUAL_CHR_SHEET_COUNT, // First 4 sheets are manually managed
            .isInUse = false
        };
    }
    
    frameCounter = 0;
    DEBUG_LOG("Virtual CHR Manager initialized\n");
}

void VirtualChrManager::Free() {
    // Nothing to clean up currently - CHR memory is managed by rendering system
    DEBUG_LOG("Virtual CHR Manager freed\n");
}

static s32 FindLRUChrSheet() {
    s32 lruIndex = -1;
    u32 oldestFrame = UINT32_MAX;
    
    // Find least recently used virtual CHR sheet (skip manually managed ones)
    for (u32 i = VIRTUAL_CHR_SHEET_START; i < CHR_COUNT; i++) {
        if (!chrSlots[i].isInUse) {
            // Prefer unused slots
            return (s32)i;
        }
        
        if (chrSlots[i].lastAccessFrame < oldestFrame) {
            oldestFrame = chrSlots[i].lastAccessFrame;
            lruIndex = (s32)i;
        }
    }
    
    return lruIndex;
}

static s32 LoadChrBankIntoSheet(ChrBankHandle bankHandle, u32 bankOffset, u32 sheetIndex) {
    if (sheetIndex >= CHR_COUNT) {
        DEBUG_ERROR("Invalid CHR sheet index: %u\n", sheetIndex);
        return -1;
    }
    
    // Copy bank tiles into the CHR sheet
    Game::Rendering::CopyBankTiles(bankHandle, bankOffset, sheetIndex, 0, CHR_SIZE_TILES);
    
    // Update slot tracking
    chrSlots[sheetIndex] = {
        .bankHandle = bankHandle,
        .bankOffset = bankOffset,
        .accessCount = chrSlots[sheetIndex].accessCount + 1,
        .lastAccessFrame = frameCounter,
        .isManuallyManaged = sheetIndex < MANUAL_CHR_SHEET_COUNT,
        .isInUse = true
    };
    
    DEBUG_LOG("Loaded CHR bank %llu (offset %u) into sheet %u\n", bankHandle.id, bankOffset, sheetIndex);
    return (s32)sheetIndex;
}

static s32 EnsureChrBankLoaded(ChrBankHandle bankHandle, u32 bankOffset) {
    if (bankHandle.id == 0) {
        return -1; // Null handle
    }
    
    frameCounter++;
    
    // Check if already loaded
    for (u32 i = 0; i < CHR_COUNT; i++) {
        if (chrSlots[i].isInUse && 
            chrSlots[i].bankHandle.id == bankHandle.id && 
            chrSlots[i].bankOffset == bankOffset) {
            // Already loaded, update access info
            chrSlots[i].accessCount++;
            chrSlots[i].lastAccessFrame = frameCounter;
            return (s32)i;
        }
    }
    
    // Find a slot to load into
    s32 targetSheet = FindLRUChrSheet();
    if (targetSheet < 0) {
        DEBUG_ERROR("No available CHR sheet for virtual loading\n");
        return -1;
    }
    
    return LoadChrBankIntoSheet(bankHandle, bankOffset, (u32)targetSheet);
}

s32 VirtualChrManager::EnsureMetaspriteChrLoaded(MetaspriteHandle metaspriteHandle) {
    const Metasprite* pMetasprite = AssetManager::GetAsset(metaspriteHandle);
    if (!pMetasprite) {
        DEBUG_ERROR("Failed to load metasprite %llu\n", metaspriteHandle.id);
        return -1;
    }
    
    return EnsureChrBankLoaded(pMetasprite->chrBankHandle, pMetasprite->chrBankOffset);
}

s32 VirtualChrManager::EnsureTilesetChrLoaded(TilesetHandle tilesetHandle) {
    const Tileset* pTileset = AssetManager::GetAsset(tilesetHandle);
    if (!pTileset) {
        DEBUG_ERROR("Failed to load tileset %llu\n", tilesetHandle.id);
        return -1;
    }
    
    return EnsureChrBankLoaded(pTileset->chrBankHandle, pTileset->chrBankOffset);
}

void VirtualChrManager::RegisterChrBankUsage(ChrBankHandle bankHandle, u32 sheetIndex) {
    if (sheetIndex >= CHR_COUNT) {
        DEBUG_ERROR("Invalid CHR sheet index: %u\n", sheetIndex);
        return;
    }
    
    frameCounter++;
    
    chrSlots[sheetIndex] = {
        .bankHandle = bankHandle,
        .bankOffset = 0, // Unknown for manually managed sheets
        .accessCount = chrSlots[sheetIndex].accessCount + 1,
        .lastAccessFrame = frameCounter,
        .isManuallyManaged = true,
        .isInUse = true
    };
}

void VirtualChrManager::TouchChrSheet(u32 sheetIndex) {
    if (sheetIndex >= CHR_COUNT) {
        return;
    }
    
    frameCounter++;
    chrSlots[sheetIndex].accessCount++;
    chrSlots[sheetIndex].lastAccessFrame = frameCounter;
}

#ifdef EDITOR
void VirtualChrManager::GetChrUsageInfo(u32 sheetIndex, ChrUsageInfo& outInfo) {
    if (sheetIndex >= CHR_COUNT) {
        outInfo = {};
        return;
    }
    
    const ChrSlot& slot = chrSlots[sheetIndex];
    outInfo = {
        .bankHandle = slot.bankHandle,
        .bankOffset = slot.bankOffset,
        .accessCount = slot.accessCount,
        .lastAccessFrame = slot.lastAccessFrame,
        .isManuallyManaged = slot.isManuallyManaged
    };
}

bool VirtualChrManager::IsChrSheetInUse(u32 sheetIndex) {
    if (sheetIndex >= CHR_COUNT) {
        return false;
    }
    
    return chrSlots[sheetIndex].isInUse;
}
#endif