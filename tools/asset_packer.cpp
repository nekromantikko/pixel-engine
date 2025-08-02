#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

// Include necessary engine headers
#include "../src/typedef.h"
#include "../src/asset_types.h"
#include "../src/asset_manager.h"
#include "../src/debug.h"

#ifdef EDITOR
#include "../src/editor_assets.h"
#include "../src/editor_serialization.h"
#include "../src/random.h"
#endif

namespace fs = std::filesystem;

struct AssetFile {
    fs::path relativePath;
    fs::path fullPath;
    AssetType type;
    std::string name;
};

static bool TryGetAssetTypeFromPath(const fs::path& path, AssetType& outType) {
    std::string extension = path.extension().string();
    
    // Map of file extensions to asset types
    if (extension == ".bmp") {
        outType = ASSET_TYPE_CHR_BANK;
        return true;
    } else if (extension == ".nsf") {
        outType = ASSET_TYPE_SOUND;
        return true;
    } else if (extension == ".tset") {
        outType = ASSET_TYPE_TILESET;
        return true;
    } else if (extension == ".sprite") {
        outType = ASSET_TYPE_METASPRITE;
        return true;
    } else if (extension == ".actor") {
        outType = ASSET_TYPE_ACTOR_PROTOTYPE;
        return true;
    } else if (extension == ".room") {
        outType = ASSET_TYPE_ROOM_TEMPLATE;
        return true;
    } else if (extension == ".dung") {
        outType = ASSET_TYPE_DUNGEON;
        return true;
    } else if (extension == ".ow") {
        outType = ASSET_TYPE_OVERWORLD;
        return true;
    } else if (extension == ".anim") {
        outType = ASSET_TYPE_ANIMATION;
        return true;
    } else if (extension == ".dat") {
        outType = ASSET_TYPE_PALETTE;
        return true;
    }
    
    return false;
}

static std::string GetAssetNameFromPath(const fs::path& path) {
    return path.stem().string();
}

static std::vector<AssetFile> DiscoverAssets(const fs::path& assetsDir) {
    std::vector<AssetFile> assets;
    
    if (!fs::exists(assetsDir) || !fs::is_directory(assetsDir)) {
        std::cerr << "Assets directory does not exist: " << assetsDir << std::endl;
        return assets;
    }
    
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
        if (entry.is_regular_file()) {
            AssetType type;
            if (TryGetAssetTypeFromPath(entry.path(), type)) {
                // Skip .meta files
                if (entry.path().extension() == ".meta") {
                    continue;
                }
                
                AssetFile asset;
                asset.fullPath = entry.path();
                // Create relative path from the assets directory for editor functions
                asset.relativePath = fs::relative(entry.path(), assetsDir);
                asset.type = type;
                asset.name = GetAssetNameFromPath(entry.path());
                
                assets.push_back(asset);
            }
        }
    }
    
    return assets;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <assets_dir> <output_file>" << std::endl;
        return 1;
    }
    
    fs::path assetsDir = argv[1];
    fs::path outputFile = argv[2];
    
    std::cout << "Asset Packer - Generating " << outputFile << " from " << assetsDir << std::endl;
    
    // Initialize random for UUID generation (required by AssetManager)
    Random::Seed(12345);
    
    // Discover all assets
    auto assets = DiscoverAssets(assetsDir);
    std::cout << "Found " << assets.size() << " assets to pack" << std::endl;
    
    // Create empty archive first
    if (!AssetManager::LoadArchive(outputFile)) {
        std::cerr << "Failed to create asset archive" << std::endl;
        return 1;
    }
    
#ifdef EDITOR
    // Process each asset
    for (const auto& asset : assets) {
        std::cout << "Processing: " << asset.relativePath << " (" << ASSET_TYPE_NAMES[asset.type] << ")" << std::endl;
        
        // The editor functions expect ASSETS_SRC_DIR to be set and to work with relative paths
        // Get asset size first  
        u32 assetSize = 0;
        if (!Editor::Assets::LoadAssetFromFile(asset.type, asset.relativePath, assetSize, nullptr)) {
            std::cerr << "Failed to get size for asset: " << asset.relativePath << std::endl;
            continue;
        }
        
        // Create asset entry
        u64 assetId = Random::GenerateUUID();
        void* pAssetData = AssetManager::AddAsset(
            assetId, 
            asset.type, 
            assetSize, 
            asset.relativePath.string().c_str(),
            asset.name.c_str(), 
            nullptr
        );
        
        if (!pAssetData) {
            std::cerr << "Failed to add asset: " << asset.relativePath << std::endl;
            continue;
        }
        
        // Load asset data into the archive
        if (!Editor::Assets::LoadAssetFromFile(asset.type, asset.relativePath, assetSize, pAssetData)) {
            std::cerr << "Failed to load asset data: " << asset.relativePath << std::endl;
            AssetManager::RemoveAsset(assetId);
            continue;
        }
        
        std::cout << "  Added asset ID: " << assetId << ", Size: " << assetSize << " bytes" << std::endl;
    }
#else
    std::cerr << "Asset packer must be built with EDITOR=ON to access asset loading functions" << std::endl;
    return 1;
#endif
    
    // Save the archive
    if (!AssetManager::SaveArchive(outputFile)) {
        std::cerr << "Failed to save asset archive" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully created " << outputFile << " with " << AssetManager::GetAssetCount() << " assets" << std::endl;
    return 0;
}