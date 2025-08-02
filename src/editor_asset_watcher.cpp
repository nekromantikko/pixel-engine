#include "editor_asset_watcher.h"
#include "editor_serialization.h"
#include "asset_manager.h"
#include "asset_types.h"
#include "debug.h"
#include "random.h"
#include <map>

static constexpr const char* ASSET_TYPE_FILE_EXTENSIONS[ASSET_TYPE_COUNT] = {
	".bmp", // CHR_BANK
	".nsf", // SOUND
	".tset", // TILESET
	".sprite", // METASPRITE
	".actor", // ACTOR_PROTOTYPE
	".room", // ROOM_TEMPLATE
	".dung", // DUNGEON
	".ow", // OVERWORLD
	".anim", // ANIMATION
	".dat", // PALETTE
};

static const std::map<std::string, AssetType> fileExtensionToAssetType = {
	{ ".bmp", ASSET_TYPE_CHR_BANK },
	{ ".nsf", ASSET_TYPE_SOUND },
	{ ".tset", ASSET_TYPE_TILESET },
	{ ".sprite", ASSET_TYPE_METASPRITE },
	{ ".actor", ASSET_TYPE_ACTOR_PROTOTYPE },
	{ ".room", ASSET_TYPE_ROOM_TEMPLATE },
	{ ".dung", ASSET_TYPE_DUNGEON },
	{ ".ow", ASSET_TYPE_OVERWORLD },
	{ ".anim", ASSET_TYPE_ANIMATION },
	{ ".dat", ASSET_TYPE_PALETTE }
};

static bool TryGetAssetTypeFromPath(const std::filesystem::path& path, AssetType& outType) {
	auto ext = path.extension().string();
	if (fileExtensionToAssetType.find(ext) != fileExtensionToAssetType.end()) {
		outType = fileExtensionToAssetType.at(ext);
		return true;
	}
	return false; // Invalid type
}

static bool HasMetadata(const std::filesystem::path& path) {
	// Check if the file has a metadata file (e.g., .meta)
	return std::filesystem::exists(Editor::Assets::GetAssetMetadataPath(path));
}

static bool CreateAssetMetadataFile(const std::filesystem::path& path, nlohmann::json& outMetadata) {
	u64 guid = Random::GenerateUUID();
	Editor::Assets::InitializeMetadataJson(outMetadata, guid);

	if (!Editor::Assets::SaveAssetMetadataToFile(path, outMetadata)) {
		DEBUG_ERROR("Failed to create metadata for %s\n", path.string().c_str());
		return false;
	}

	DEBUG_LOG("Created metadata for %s with GUID: %llu\n", path.string().c_str(), guid);
	return true;
}

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
            std::string pathStr = entry.path().string();
            const char* pathCStr = pathStr.c_str();
			DEBUG_LOG("Found %s: %s\n", ASSET_TYPE_NAMES[assetType], pathCStr);

			u64 guid = UUID_NULL;
			nlohmann::json metadata;

			if (HasMetadata(entry.path())) {
				DEBUG_LOG("[META FILE FOUND]\n");
				if (LoadAssetMetadataFromFile(entry.path(), metadata)) {
					guid = metadata["guid"];
					DEBUG_LOG("Metadata loaded with GUID: %llu\n", guid);
				}
				else {
					DEBUG_ERROR("Failed to load metadata for %s\n", pathCStr);
				}
			}
			else {
				DEBUG_ERROR("[!! META FILE MISSING !!]\n");
				if (CreateAssetMetadataFile(entry.path(), metadata)) {		
					guid = metadata["guid"];
					DEBUG_LOG("Metadata created with GUID: %llu\n", guid);
				}
				else {
					DEBUG_ERROR("Failed to create metadata for %s\n", pathCStr);
				}
			}

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

            void* pData = AssetManager::AddAsset(guid, assetType, size, name.c_str(), nullptr);
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