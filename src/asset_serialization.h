#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include "typedef.h"
#include "asset_types.h"

enum SerializationResult {
	SERIALIZATION_SUCCESS = 0,
	SERIALIZATION_FILE_NOT_FOUND,
	SERIALIZATION_FAILED_TO_OPEN_FILE,
	SERIALIZATION_INVALID_ASSET_TYPE,
	SERIALIZATION_NULL_POINTER,
	SERIALIZATION_INVALID_METADATA,
	SERIALIZATION_INVALID_ASSET_DATA,
	SERIALIZATION_NOT_IMPLEMENTED,
	SERIALIZATION_UNKNOWN_ERROR,

	SERIALIZATION_RESULT_COUNT
};

namespace AssetSerialization {
	SerializationResult TryGetAssetTypeFromPath(const std::filesystem::path& path, AssetType& outType);
	bool HasMetadata(const std::filesystem::path& path);
	std::filesystem::path GetAssetMetadataPath(const std::filesystem::path& path);

	SerializationResult LoadAssetMetadataFromFile(const std::filesystem::path& origPath, nlohmann::json& outJson);
	SerializationResult SaveAssetMetadataToFile(const std::filesystem::path& origPath, const nlohmann::json& json);
	void InitializeMetadataJson(nlohmann::json& json, u64 id);
	SerializationResult CreateAssetMetadataFile(const std::filesystem::path& path, u64 guid, nlohmann::json& outMetadata);

	SerializationResult LoadAssetFromFile(const std::filesystem::path& path, AssetType type, const nlohmann::json& metadata, size_t& size, void* pOutData);
	SerializationResult SaveAssetToFile(const std::filesystem::path& path, const char* name, AssetType type, nlohmann::json& metadata, const void* pData);
}