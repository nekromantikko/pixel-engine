#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include "debug.h"
#include "typedef.h"
#include "asset_types.h"

namespace Editor::Assets {
	std::filesystem::path GetAssetMetadataPath(const std::filesystem::path& path);
	bool LoadAssetMetadataFromFile(const std::filesystem::path& origPath, nlohmann::json& outJson);
	bool SaveAssetMetadataToFile(const std::filesystem::path& origPath, const nlohmann::json& json);
	void InitializeMetadataJson(nlohmann::json& json, u64 id);

	bool LoadAssetFromFile(const std::filesystem::path& path, AssetType type, const nlohmann::json& metadata, u32& size, void* pOutData);
	bool SaveAssetToFile(const std::filesystem::path& path, AssetType type, const void* pData);
	
	std::filesystem::path GenerateAssetPath(AssetType type, const char* name);
	bool SerializeAssetToFile(u64 assetId, AssetType type, const void* pData, const char* relativePath = nullptr);
}