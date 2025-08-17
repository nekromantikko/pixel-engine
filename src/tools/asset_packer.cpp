#include <iostream>
#include <filesystem>
#include "../asset_archive.h"
#include "../asset_serialization.h"
#include "../shader_compiler.h"
#include <fstream>

namespace fs = std::filesystem;

// TODO: Remove copypasta from AssetManager
bool PackAssetsFromDirectory(AssetArchive& archive, const fs::path& directory) {
	if (!std::filesystem::exists(directory)) {
		std::cerr << "Directory (" << directory.string() << ") does not exist" << std::endl;
		return false;
	}

	if (!std::filesystem::is_directory(directory)) {
		std::cerr << "Path (" << directory.string() << ") is not a directory" << std::endl;
		return false;
	}

	std::cout << "Packing assets from directory: " << directory.string() << std::endl;

	std::vector<u8> data;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
		AssetType assetType;
		if (entry.is_regular_file() && AssetSerialization::TryGetAssetTypeFromPath(entry.path(), assetType) == SERIALIZATION_SUCCESS) {
			const std::string pathStr = entry.path().string();
			const char* pathCStr = pathStr.c_str();
			std::cout << "Found " << ASSET_TYPE_NAMES[assetType] << ": " << pathCStr << std::endl;

			nlohmann::json metadata;
			if (AssetSerialization::LoadAssetMetadataFromFile(entry.path(), metadata) != SERIALIZATION_SUCCESS) {
				std::cerr << "Failed to load metadata for asset " << pathCStr << std::endl;
				continue;
			}

			const u64 guid = metadata["guid"];

			data.clear();
			if (AssetSerialization::LoadAssetFromFile(entry.path(), assetType, metadata, data) != SERIALIZATION_SUCCESS) {
				std::cerr << "Failed to load asset data from " << pathCStr << std::endl;
				continue;
			}

			const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), directory);
			if (!archive.AddAsset(guid, assetType, data.size(), relativePath.string().c_str(),  data.data())) {
				std::cerr << "Failed to add asset " << pathCStr << " to archive" << std::endl;
				continue;
			}
			std::cout << "Asset " << pathCStr << " packed successfully with GUID: " << guid << std::endl;
		}
	}

	return true;
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <assets_dir> <output_file>" << std::endl;
		return 1;
	}

	fs::path assetsDir = argv[1];
	fs::path outputFile = argv[2];

	std::cout << "Asset Packer - Generating " << outputFile << " from " << assetsDir << std::endl;

	AssetArchive archive;
	if (!PackAssetsFromDirectory(archive, assetsDir)) {
		std::cerr << "Failed to pack assets from " << assetsDir << std::endl;
		return 1;
	}
	
	if (!archive.SaveToFile(outputFile)) {
		std::cerr << "Failed to save assets to " << outputFile << std::endl;
		return 1;
	}

	std::cout << "Assets successfully packed into " << outputFile << std::endl;

	// This isn't necessary, but I suppose it doesn't hurt to explicity free the memory
	archive.Clear();

	return 0;
}