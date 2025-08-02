#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <cstdint>

// Basic type definitions
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s16 = int16_t;

// Asset types from the engine
enum AssetType : u8 {
    ASSET_TYPE_CHR_BANK,
    ASSET_TYPE_SOUND,
    ASSET_TYPE_TILESET,
    ASSET_TYPE_METASPRITE,
    ASSET_TYPE_ACTOR_PROTOTYPE,
    ASSET_TYPE_ROOM_TEMPLATE,
    ASSET_TYPE_DUNGEON,
    ASSET_TYPE_OVERWORLD,
    ASSET_TYPE_ANIMATION,
    ASSET_TYPE_PALETTE,
    ASSET_TYPE_COUNT,
};

// Basic structures for NPAK format
struct ArchiveHeader {
    char signature[4];
    u32 assetCount;
    u32 directoryOffset;
};

struct AssetFlags {
    AssetType type : 4;
    bool deleted : 1;
    bool compressed : 1;
};

struct AssetEntry {
    u64 id;
    char name[56];
    char relativePath[256];
    u32 offset;
    u32 size;
    AssetFlags flags;
};

// Basic sprite structure
struct Sprite {
    s16 x, y;
    u16 tileId;
    u8 palette;
    u8 priority : 1;
    u8 flipHorizontal : 1;
    u8 flipVertical : 1;
};

struct Metasprite {
    u32 spriteCount;
    // Sprites follow immediately after this header
};

namespace fs = std::filesystem;

struct AssetFile {
    fs::path relativePath;
    fs::path fullPath;
    AssetType type;
    std::string name;
    u64 id;
};

static bool TryGetAssetTypeFromPath(const fs::path& path, AssetType& outType) {
    std::string extension = path.extension().string();
    
    if (extension == ".bmp") {
        outType = ASSET_TYPE_CHR_BANK;
        return true;
    } else if (extension == ".sprite") {
        outType = ASSET_TYPE_METASPRITE;
        return true;
    } else if (extension == ".dat") {
        outType = ASSET_TYPE_PALETTE;
        return true;
    }
    // Add other simple types as needed
    
    return false;
}

static std::string GetAssetNameFromPath(const fs::path& path) {
    return path.stem().string();
}

static bool ReadJsonFile(const fs::path& path, std::string& content) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    content.assign((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());
    return true;
}

static bool GetAssetIdFromMeta(const fs::path& assetPath, u64& outId) {
    fs::path metaPath = assetPath.string() + ".meta";
    
    if (!fs::exists(metaPath)) {
        std::cerr << "Missing meta file: " << metaPath << std::endl;
        return false;
    }
    
    std::string content;
    if (!ReadJsonFile(metaPath, content)) {
        return false;
    }
    
    // Simple JSON parsing for guid field
    size_t guidPos = content.find("\"guid\":");
    if (guidPos == std::string::npos) {
        return false;
    }
    
    size_t valueStart = content.find_first_of("0123456789", guidPos);
    if (valueStart == std::string::npos) {
        return false;
    }
    
    size_t valueEnd = content.find_first_not_of("0123456789", valueStart);
    std::string guidStr = content.substr(valueStart, valueEnd - valueStart);
    
    try {
        outId = std::stoull(guidStr);
        return true;
    } catch (...) {
        return false;
    }
}

static bool ProcessSpriteAsset(const AssetFile& asset, std::vector<u8>& data) {
    std::string content;
    if (!ReadJsonFile(asset.fullPath, content)) {
        return false;
    }
    
    // Simple JSON parsing for sprites array
    size_t spritesPos = content.find("\"sprites\":");
    if (spritesPos == std::string::npos) {
        return false;
    }
    
    // Count sprites by counting objects in the sprites array
    size_t arrayStart = content.find("[", spritesPos);
    size_t arrayEnd = content.find("]", arrayStart);
    if (arrayStart == std::string::npos || arrayEnd == std::string::npos) {
        return false;
    }
    
    // Count the number of sprite objects
    u32 spriteCount = 0;
    size_t pos = arrayStart;
    while ((pos = content.find("{", pos + 1)) != std::string::npos && pos < arrayEnd) {
        spriteCount++;
    }
    
    // Create metasprite data
    u32 totalSize = sizeof(Metasprite) + spriteCount * sizeof(Sprite);
    data.resize(totalSize);
    
    Metasprite* meta = reinterpret_cast<Metasprite*>(data.data());
    meta->spriteCount = spriteCount;
    
    Sprite* sprites = reinterpret_cast<Sprite*>(data.data() + sizeof(Metasprite));
    
    // Parse each sprite (very basic parsing - would need proper JSON parser for production)
    // For now, just zero-initialize the sprites
    memset(sprites, 0, spriteCount * sizeof(Sprite));
    
    std::cout << "  Processed sprite with " << spriteCount << " sprites" << std::endl;
    return true;
}

static bool ProcessPaletteAsset(const AssetFile& asset, std::vector<u8>& data) {
    // Read the palette file as binary data
    std::ifstream file(asset.fullPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    std::cout << "  Processed palette (" << size << " bytes)" << std::endl;
    return true;
}

static bool ProcessChrBankAsset(const AssetFile& asset, std::vector<u8>& data) {
    // Read the BMP file as binary data (simplified - real version would parse BMP properly)
    std::ifstream file(asset.fullPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    std::cout << "  Processed CHR bank (" << size << " bytes)" << std::endl;
    return true;
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
                asset.relativePath = fs::relative(entry.path(), assetsDir);
                asset.type = type;
                asset.name = GetAssetNameFromPath(entry.path());
                
                if (!GetAssetIdFromMeta(entry.path(), asset.id)) {
                    std::cerr << "Failed to get ID for asset: " << entry.path() << std::endl;
                    continue;
                }
                
                assets.push_back(asset);
            }
        }
    }
    
    return assets;
}

static bool CreateNPAK(const std::vector<AssetFile>& assets, const fs::path& outputPath) {
    std::vector<std::vector<u8>> assetDataList;
    std::vector<AssetEntry> entries;
    
    u32 dataOffset = 0;
    
    // Process each asset
    for (const auto& asset : assets) {
        std::vector<u8> assetData;
        bool success = false;
        
        switch (asset.type) {
            case ASSET_TYPE_METASPRITE:
                success = ProcessSpriteAsset(asset, assetData);
                break;
            case ASSET_TYPE_PALETTE:
                success = ProcessPaletteAsset(asset, assetData);
                break;
            case ASSET_TYPE_CHR_BANK:
                success = ProcessChrBankAsset(asset, assetData);
                break;
            default:
                std::cout << "  Skipping unsupported asset type" << std::endl;
                continue;
        }
        
        if (!success) {
            std::cerr << "Failed to process asset: " << asset.relativePath << std::endl;
            continue;
        }
        
        // Create asset entry
        AssetEntry entry = {};
        entry.id = asset.id;
        strncpy(entry.name, asset.name.c_str(), sizeof(entry.name) - 1);
        strncpy(entry.relativePath, asset.relativePath.string().c_str(), sizeof(entry.relativePath) - 1);
        entry.offset = dataOffset;
        entry.size = assetData.size();
        entry.flags.type = asset.type;
        entry.flags.deleted = false;
        entry.flags.compressed = false;
        
        entries.push_back(entry);
        assetDataList.push_back(std::move(assetData));
        dataOffset += entry.size;
        
        std::cout << "  Added asset " << asset.name << " (ID: " << asset.id << ", Size: " << entry.size << ")" << std::endl;
    }
    
    // Write NPAK file
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return false;
    }
    
    // Write header
    ArchiveHeader header = {};
    header.signature[0] = 'N';
    header.signature[1] = 'P';
    header.signature[2] = 'A';
    header.signature[3] = 'K';
    header.assetCount = entries.size();
    header.directoryOffset = sizeof(ArchiveHeader) + dataOffset;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write asset data
    for (const auto& assetData : assetDataList) {
        file.write(reinterpret_cast<const char*>(assetData.data()), assetData.size());
    }
    
    // Write directory
    for (const auto& entry : entries) {
        file.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <assets_dir> <output_file>" << std::endl;
        return 1;
    }
    
    fs::path assetsDir = argv[1];
    fs::path outputFile = argv[2];
    
    std::cout << "Simple Asset Packer - Generating " << outputFile << " from " << assetsDir << std::endl;
    
    // Discover all assets
    auto assets = DiscoverAssets(assetsDir);
    std::cout << "Found " << assets.size() << " assets to pack" << std::endl;
    
    if (assets.empty()) {
        std::cerr << "No assets found!" << std::endl;
        return 1;
    }
    
    // Create NPAK file
    if (!CreateNPAK(assets, outputFile)) {
        std::cerr << "Failed to create NPAK file" << std::endl;
        return 1;
    }
    
    std::cout << "Successfully created " << outputFile << " with " << assets.size() << " assets" << std::endl;
    return 0;
}