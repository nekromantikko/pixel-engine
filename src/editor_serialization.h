#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include "debug.h"

static constexpr u64 ASSET_FILE_FORMAT_VERSION = 1;

struct SerializedAssetMetadata {
	u64 fileFormatVersion = ASSET_FILE_FORMAT_VERSION;
	u64 guid;
};

static void from_json(const nlohmann::json& j, SerializedAssetMetadata& metadata) {
	j.at("file_format_version").get_to(metadata.fileFormatVersion);
	j.at("guid").get_to(metadata.guid);
}

static void to_json(nlohmann::json& j, const SerializedAssetMetadata& metadata) {
	j["file_format_version"] = metadata.fileFormatVersion;
	j["guid"] = metadata.guid;
}

namespace Editor {
	bool LoadSerializedAssetFromFile(const std::filesystem::path& path, nlohmann::json& outJson) {
		if (!std::filesystem::exists(path)) {
			DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
			return false;
		}

		FILE* pFile = fopen(path.string().c_str(), "rb");
		if (!pFile) {
			DEBUG_ERROR("Failed to open file\n");
			return false;
		}

		outJson = nlohmann::json::parse(pFile);
		fclose(pFile);

		return true;
	}

	bool SaveSerializedAssetMetadataToFile(const std::filesystem::path& origPath, const SerializedAssetMetadata& metadata) {
        std::filesystem::path path = origPath;
        path += ".meta";

		nlohmann::json json;
		json["file_format_version"] = metadata.fileFormatVersion;
		json["guid"] = metadata.guid;

		FILE* pFile = fopen(path.string().c_str(), "wb");
		if (!pFile) {
			DEBUG_ERROR("Failed to open file for writing\n");
			return false;
		}

		fwrite(json.dump(4).c_str(), sizeof(char), json.dump(4).size(), pFile);
		fclose(pFile);

		return true;
	}

	bool SaveSerializedAssetToFile(const std::filesystem::path& path, const nlohmann::json& json, const u64 id) {
		if (!SaveSerializedAssetMetadataToFile(path, { ASSET_FILE_FORMAT_VERSION, id })) {
			DEBUG_ERROR("Failed to save metadata for asset\n");
			return false;
		}

		FILE* pFile = fopen(path.string().c_str(), "wb");
		if (!pFile) {
			DEBUG_ERROR("Failed to open file for writing\n");
			return false;
		}

		fwrite(json.dump(4).c_str(), sizeof(char), json.dump(4).size(), pFile);
		fclose(pFile);

		return true;
	}
}