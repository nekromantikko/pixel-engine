#include "editor_asset_watcher.h"
#include "editor_assets.h"
#include "editor_serialization.h"
#include "asset_manager.h"
#include "asset_types.h"
#include "debug.h"

bool Editor::Assets::ListFilesInDirectory(const std::filesystem::path& directory) {
	if (!std::filesystem::exists(directory)) {
		DEBUG_ERROR("Directory (%s) does not exist\n", directory.string().c_str());
		return false;
	}

	if (!std::filesystem::is_directory(directory)) {
		DEBUG_ERROR("Path (%s) is not a directory\n", directory.string().c_str());
		return false;
	}

	DEBUG_LOG("Listing files in directory: %s\n", directory.string().c_str());

	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
		AssetType assetType;
		if (entry.is_regular_file() && TryGetAssetTypeFromPath(entry.path(), assetType)) {
            const std::string pathStr = entry.path().string();
            const char* pathCStr = pathStr.c_str();
			DEBUG_LOG("Found %s: %s\n", ASSET_TYPE_NAMES[assetType], pathCStr);

			nlohmann::json metadata;
			if (!TryGetAssetMetadata(entry.path(), metadata)) {
				DEBUG_ERROR("Failed to load metadata for asset %s\n", pathCStr);
				continue;
			}
			
			const u64 guid = metadata["guid"];

			u32 size;
			if (!LoadAssetFromFile(entry.path(), assetType, metadata, size, nullptr)) {
				DEBUG_ERROR("Failed to get size for asset %s\n", pathCStr);
				continue;
			}
            const std::string filenameWithoutExt = entry.path().filename().replace_extension("").string();
			std::string name = filenameWithoutExt;
			if (metadata.contains("name") && !metadata["name"].is_null()) 
			{
				name = metadata["name"].get<std::string>();
			}

			const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), ASSETS_SRC_DIR);
            void* pData = AssetManager::AddAsset(guid, assetType, size, relativePath.string().c_str(), name.c_str(), nullptr);
			if (!pData) {
				DEBUG_ERROR("Failed to add asset %s to manager\n", pathCStr);
				continue;
			}
			if (!LoadAssetFromFile(entry.path(), assetType, metadata, size, pData)) {
				DEBUG_ERROR("Failed to load asset data from %s\n", pathCStr);
				AssetManager::RemoveAsset(guid);
				continue;
			}
			DEBUG_LOG("Asset %s loaded successfully with GUID: %llu\n", pathCStr, guid);
		}
	}

	return true;
}