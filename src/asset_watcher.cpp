#ifdef EDITOR

#include "asset_watcher.h"
#include "debug.h"
#include "asset_manager.h"
#include "asset_serialization.h"
#include "editor_assets.h"
#include "random.h"
#include <algorithm>
#include <vector>

namespace AssetWatcher {
    // Static variables for the watcher state
    static std::filesystem::path s_watchDirectory;
    static std::unordered_map<std::string, FileInfo> s_fileMap;
    static std::vector<FileChangeEvent> s_pendingChanges;
    static bool s_isWatching = false;
    static bool s_initialized = false;

    void Init(const std::filesystem::path& assetsDir) {
        if (s_initialized) {
            Free();
        }

        s_watchDirectory = assetsDir;
        s_fileMap.clear();
        s_pendingChanges.clear();
        s_isWatching = false;
        s_initialized = true;

        if (!std::filesystem::exists(s_watchDirectory)) {
            DEBUG_ERROR("Asset watcher: Watch directory does not exist: %s", s_watchDirectory.string().c_str());
            return;
        }

        // Initial scan to populate the file map
        ScanDirectory();
        s_isWatching = true;

        DEBUG_LOG("Asset watcher initialized for directory: %s", s_watchDirectory.string().c_str());
    }

    void Free() {
        if (!s_initialized) {
            return;
        }

        s_isWatching = false;
        s_fileMap.clear();
        s_pendingChanges.clear();
        s_initialized = false;

        DEBUG_LOG("Asset watcher freed");
    }

    void Update() {
        if (!s_isWatching || !s_initialized) {
            return;
        }

        ScanDirectory();
        ProcessFileChanges();
    }

    const std::filesystem::path& GetWatchDirectory() {
        return s_watchDirectory;
    }

    bool IsWatching() {
        return s_isWatching && s_initialized;
    }

    void ScanDirectory() {
        std::unordered_map<std::string, FileInfo> newFileMap;

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(s_watchDirectory)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const std::string pathStr = entry.path().string();
                
                // Only track asset files and meta files
                if (!IsAssetFile(entry.path()) && !IsMetaFile(entry.path())) {
                    continue;
                }

                FileInfo info;
                info.lastWriteTime = entry.last_write_time();
                info.size = entry.file_size();
                info.exists = true;

                newFileMap[pathStr] = info;
            }
        }
        catch (const std::filesystem::filesystem_error& ex) {
            DEBUG_ERROR("Asset watcher: Error scanning directory: %s", ex.what());
            return;
        }

        // Compare with previous scan to detect changes
        for (const auto& [pathStr, info] : newFileMap) {
            std::filesystem::path path(pathStr);
            
            auto oldIt = s_fileMap.find(pathStr);
            if (oldIt == s_fileMap.end()) {
                // File was added
                s_pendingChanges.push_back({ path, ADDED, {} });
            }
            else if (oldIt->second.lastWriteTime != info.lastWriteTime || 
                     oldIt->second.size != info.size) {
                // File was modified
                s_pendingChanges.push_back({ path, MODIFIED, {} });
            }
        }

        // Check for deleted files
        for (const auto& [pathStr, info] : s_fileMap) {
            if (newFileMap.find(pathStr) == newFileMap.end()) {
                // File was deleted
                s_pendingChanges.push_back({ std::filesystem::path(pathStr), DELETED, {} });
            }
        }

        s_fileMap = std::move(newFileMap);
    }

    void ProcessFileChanges() {
        for (const auto& change : s_pendingChanges) {
            switch (change.changeType) {
                case ADDED:
                    HandleFileAdded(change.path);
                    break;
                case DELETED:
                    HandleFileDeleted(change.path);
                    break;
                case MODIFIED:
                    // For now, treat modifications as delete + add
                    HandleFileDeleted(change.path);
                    HandleFileAdded(change.path);
                    break;
                case RENAMED:
                    HandleFileRenamed(change.oldPath, change.path);
                    break;
            }
        }
        s_pendingChanges.clear();
    }

    void HandleFileAdded(const std::filesystem::path& path) {
        DEBUG_LOG("Asset watcher: File added: %s", path.string().c_str());

        // Skip if this is a meta file - we only process asset files
        if (IsMetaFile(path)) {
            return;
        }

        // Check if this is a valid asset file
        AssetType assetType;
        if (!IsAssetFile(path) || AssetSerialization::TryGetAssetTypeFromPath(path, assetType) != SERIALIZATION_SUCCESS) {
            return;
        }

        try {
            nlohmann::json metadata;
            u64 guid;

            // Check if metadata file exists
            if (!AssetSerialization::HasMetadata(path)) {
                DEBUG_LOG("Asset watcher: Creating metadata for new asset: %s", path.string().c_str());
                guid = Random::GenerateUUID();
                if (AssetSerialization::CreateAssetMetadataFile(path, guid, metadata) != SERIALIZATION_SUCCESS) {
                    DEBUG_ERROR("Asset watcher: Failed to create metadata for asset: %s", path.string().c_str());
                    return;
                }
            }
            else {
                // Load existing metadata
                if (AssetSerialization::LoadAssetMetadataFromFile(path, metadata) != SERIALIZATION_SUCCESS) {
                    DEBUG_ERROR("Asset watcher: Failed to load metadata for asset: %s", path.string().c_str());
                    return;
                }
                guid = metadata["guid"];
            }

            // Load the asset data
            std::vector<u8> data;
            if (AssetSerialization::LoadAssetFromFile(path, assetType, metadata, data) != SERIALIZATION_SUCCESS) {
                DEBUG_ERROR("Asset watcher: Failed to load asset data: %s", path.string().c_str());
                return;
            }

            // Get asset name from metadata or filename
            std::string name = path.filename().replace_extension("").string();
            if (metadata.contains("name") && !metadata["name"].is_null()) {
                name = metadata["name"].get<std::string>();
            }

            // Add to asset manager
            const std::filesystem::path relativePath = std::filesystem::relative(path, s_watchDirectory);
            if (!AssetManager::AddAsset(guid, assetType, data.size(), relativePath.string().c_str(), name.c_str(), data.data())) {
                DEBUG_ERROR("Asset watcher: Failed to add asset to manager: %s", path.string().c_str());
                return;
            }

            DEBUG_LOG("Asset watcher: Successfully added asset: %s with GUID: %llu", path.string().c_str(), guid);
        }
        catch (const std::exception& ex) {
            DEBUG_ERROR("Asset watcher: Exception while adding asset: %s - %s", path.string().c_str(), ex.what());
        }
    }

    void HandleFileDeleted(const std::filesystem::path& path) {
        DEBUG_LOG("Asset watcher: File deleted: %s", path.string().c_str());

        // Handle meta file deletion - clean up orphaned meta files
        if (IsMetaFile(path)) {
            std::filesystem::path assetPath = GetCorrespondingAssetFile(path);
            if (!std::filesystem::exists(assetPath)) {
                DEBUG_LOG("Asset watcher: Cleaning up orphaned meta file: %s", path.string().c_str());
                // Meta file will be automatically removed by filesystem
            }
            return;
        }

        // Handle asset file deletion
        if (!IsAssetFile(path)) {
            return;
        }

        try {
            // Find the asset by its relative path
            const std::filesystem::path relativePath = std::filesystem::relative(path, s_watchDirectory);
            AssetType assetType;
            if (AssetSerialization::TryGetAssetTypeFromPath(path, assetType) != SERIALIZATION_SUCCESS) {
                return;
            }

            u64 assetId = AssetManager::GetAssetId(relativePath, assetType);
            if (assetId != UUID_NULL) {
                // Remove from asset manager
                if (AssetManager::RemoveAsset(assetId)) {
                    DEBUG_LOG("Asset watcher: Successfully removed asset: %s (ID: %llu)", path.string().c_str(), assetId);
                }
                else {
                    DEBUG_ERROR("Asset watcher: Failed to remove asset: %s", path.string().c_str());
                }
            }

            // Clean up orphaned meta file
            std::filesystem::path metaPath = GetCorrespondingMetaFile(path);
            if (std::filesystem::exists(metaPath)) {
                std::error_code ec;
                if (std::filesystem::remove(metaPath, ec)) {
                    DEBUG_LOG("Asset watcher: Cleaned up orphaned meta file: %s", metaPath.string().c_str());
                }
                else {
                    DEBUG_ERROR("Asset watcher: Failed to clean up meta file: %s - %s", metaPath.string().c_str(), ec.message().c_str());
                }
            }
        }
        catch (const std::exception& ex) {
            DEBUG_ERROR("Asset watcher: Exception while deleting asset: %s - %s", path.string().c_str(), ex.what());
        }
    }

    void HandleFileRenamed(const std::filesystem::path& oldPath, const std::filesystem::path& newPath) {
        DEBUG_LOG("Asset watcher: File renamed from %s to %s", oldPath.string().c_str(), newPath.string().c_str());

        // For simplicity, treat rename as delete + add
        HandleFileDeleted(oldPath);
        HandleFileAdded(newPath);
    }

    bool IsAssetFile(const std::filesystem::path& path) {
        AssetType assetType;
        return AssetSerialization::TryGetAssetTypeFromPath(path, assetType) == SERIALIZATION_SUCCESS;
    }

    bool IsMetaFile(const std::filesystem::path& path) {
        return path.extension() == ".meta";
    }

    std::filesystem::path GetCorrespondingMetaFile(const std::filesystem::path& assetPath) {
        return assetPath.string() + ".meta";
    }

    std::filesystem::path GetCorrespondingAssetFile(const std::filesystem::path& metaPath) {
        std::string assetPathStr = metaPath.string();
        if (assetPathStr.ends_with(".meta")) {
            assetPathStr = assetPathStr.substr(0, assetPathStr.length() - 5);
        }
        return std::filesystem::path(assetPathStr);
    }
}

#endif // EDITOR