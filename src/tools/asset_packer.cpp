#include <iostream>
#include <filesystem>
#include "asset_manager.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <assets_dir> <output_file>" << std::endl;
		return 1;
	}

	fs::path assetsDir = argv[1];
	fs::path outputFile = argv[2];

	std::cout << "Asset Packer - Generating " << outputFile << " from " << assetsDir << std::endl;

	AssetManager::LoadAssetsFromDirectory(assetsDir);
	if (!AssetManager::SaveArchive(outputFile)) {
		std::cerr << "Failed to save assets to " << outputFile << std::endl;
		return 1;
	}

	std::cout << "Assets successfully packed into " << outputFile << std::endl;
	return 0;
}