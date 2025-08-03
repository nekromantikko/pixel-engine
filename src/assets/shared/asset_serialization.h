#pragma once
#ifdef EDITOR
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include "debug.h"
#include "typedef.h"
#include "asset_types.h"

static const char* ASSET_TYPE_FILE_EXTENSIONS[ASSET_TYPE_COUNT] = {
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

namespace AssetManager::Serialization {
	bool TryGetAssetTypeFromPath(const std::filesystem::path& path, AssetType& outType);
	bool HasMetadata(const std::filesystem::path& path);
	std::filesystem::path GetAssetMetadataPath(const std::filesystem::path& path);
	std::filesystem::path GetAssetFullPath(const std::filesystem::path& relativePath);

	bool LoadAssetMetadataFromFile(const std::filesystem::path& origPath, nlohmann::json& outJson);
	bool SaveAssetMetadataToFile(const std::filesystem::path& origPath, const nlohmann::json& json);
	void InitializeMetadataJson(nlohmann::json& json, u64 id);
	bool CreateAssetMetadataFile(const std::filesystem::path& path, nlohmann::json& outMetadata);
	bool TryGetAssetMetadata(const std::filesystem::path& path, nlohmann::json& outMetadata);

	bool LoadAssetFromFile(const std::filesystem::path& path, AssetType type, const nlohmann::json& metadata, u32& size, void* pOutData);
	bool LoadAssetFromFile(AssetType type, const std::filesystem::path& relativePath, u32& size, void* pOutData);
	bool SaveAssetToFile(const std::filesystem::path& path, const char* name, AssetType type, nlohmann::json& metadata, const void* pData);
	bool SaveAssetToFile(AssetType type, const std::filesystem::path& relativePath, const char* name, const void* pData);
}
#endif