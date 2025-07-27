#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include "debug.h"
#include "typedef.h"

static constexpr u64 ASSET_FILE_FORMAT_VERSION = 1;

struct SerializedAssetMetadata {
	u64 fileFormatVersion = ASSET_FILE_FORMAT_VERSION;
	u64 guid;
};

inline void from_json(const nlohmann::json& j, SerializedAssetMetadata& metadata) {
	j.at("file_format_version").get_to(metadata.fileFormatVersion);
	j.at("guid").get_to(metadata.guid);
}

inline void to_json(nlohmann::json& j, const SerializedAssetMetadata& metadata) {
	j["file_format_version"] = metadata.fileFormatVersion;
	j["guid"] = metadata.guid;
}

namespace Editor {
	std::filesystem::path GetAssetMetadataPath(const std::filesystem::path& path);
	bool SaveSerializedAssetMetadataToFile(const std::filesystem::path& origPath, const SerializedAssetMetadata& metadata);

	bool LoadSerializedAssetFromFile(const std::filesystem::path& path, nlohmann::json& outJson);
	bool SaveSerializedAssetToFile(const std::filesystem::path& path, const nlohmann::json& json, const u64 id);
}