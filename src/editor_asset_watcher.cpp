#include "editor_asset_watcher.h"
#include "editor_serialization.h"
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
	return std::filesystem::exists(Editor::GetAssetMetadataPath(path));
}

static u64 CreateAssetMetadataFile(const std::filesystem::path& path) {
	SerializedAssetMetadata metadata;
	metadata.fileFormatVersion = ASSET_FILE_FORMAT_VERSION;
	metadata.guid = Random::GenerateUUID();

	if (!Editor::SaveSerializedAssetMetadataToFile(path, metadata)) {
		DEBUG_ERROR("Failed to create metadata for %s\n", path.string().c_str());
		return UUID_NULL;
	}

	DEBUG_LOG("Created metadata for %s with GUID: %llu\n", path.string().c_str(), metadata.guid);
	return metadata.guid;
}

bool Editor::ListFilesInDirectory(const std::filesystem::path& directory) {
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
			if (HasMetadata(entry.path())) {
				DEBUG_LOG("[META FILE FOUND]\n");
			}
			else {
				DEBUG_ERROR("[!! META FILE MISSING !!]\n");
				u64 guid = CreateAssetMetadataFile(entry.path());
				if (guid == UUID_NULL) {
					DEBUG_ERROR("Failed to create metadata for %s\n", pathCStr);
				}
				else {
					DEBUG_LOG("Metadata created with GUID: %llu\n", guid);
				}
			}
		}
	}

	return true;
}