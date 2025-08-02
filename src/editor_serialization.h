#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include "debug.h"
#include "typedef.h"
#include "asset_types.h"

namespace Editor::Assets {
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