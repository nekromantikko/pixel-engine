#include "editor_serialization.h"

std::filesystem::path Editor::GetAssetMetadataPath(const std::filesystem::path& path) {
	// Append ".meta" to the original path to get the metadata file path
	return path.string() + ".meta";
}

bool Editor::SaveSerializedAssetMetadataToFile(const std::filesystem::path& origPath, const SerializedAssetMetadata& metadata) {
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

bool Editor::LoadSerializedAssetFromFile(const std::filesystem::path& path, nlohmann::json& outJson) {
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

bool Editor::SaveSerializedAssetToFile(const std::filesystem::path& path, const nlohmann::json& json, const u64 id) {
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