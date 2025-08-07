#pragma once
#include "core_types.h"
#include "asset_types.h"

namespace VirtualChrManager {
    // Initialize the virtual CHR manager
    void Init();
    
    // Cleanup virtual CHR manager resources
    void Free();
    
    // Ensure CHR tiles for a metasprite are loaded into CHR memory
    // Returns the CHR sheet index where the tiles are loaded, or -1 if failed
    s32 EnsureMetaspriteChrLoaded(MetaspriteHandle metaspriteHandle);
    
    // Ensure CHR tiles for a tileset are loaded into CHR memory  
    // Returns the CHR sheet index where the tiles are loaded, or -1 if failed
    s32 EnsureTilesetChrLoaded(TilesetHandle tilesetHandle);
    
    // Manually register CHR bank usage (for existing manual systems)
    void RegisterChrBankUsage(ChrBankHandle bankHandle, u32 sheetIndex);
    
    // Update LRU tracking for a CHR sheet that was accessed
    void TouchChrSheet(u32 sheetIndex);
    
    // Get debug information about CHR usage
    struct ChrUsageInfo {
        ChrBankHandle bankHandle;
        u32 bankOffset;
        u32 accessCount;
        u32 lastAccessFrame;
        bool isManuallyManaged;
    };
    
    #ifdef EDITOR
    // Get CHR usage information for editor display
    void GetChrUsageInfo(u32 sheetIndex, ChrUsageInfo& outInfo);
    bool IsChrSheetInUse(u32 sheetIndex);
    #endif
}