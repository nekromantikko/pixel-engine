#pragma once
#include "asset_types.h"
#include <filesystem>
#include <map>

inline static const char* ASSET_TYPE_FILE_EXTENSIONS[ASSET_TYPE_COUNT] = {
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

inline static const std::map<std::string, AssetType> fileExtensionToAssetType = {
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

namespace Editor::Assets {
	bool TryGetAssetTypeFromPath(const std::filesystem::path& path, AssetType& outType);
	bool HasMetadata(const std::filesystem::path& path);
	std::filesystem::path GetAssetMetadataPath(const std::filesystem::path& path);
	std::filesystem::path GetAssetFullPath(const std::filesystem::path& relativePath);

	void InitializeAsset(AssetType type, void* pData);
	u32 GetAssetSize(AssetType type, const void* pData);
}