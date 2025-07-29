#pragma once
#include <filesystem>

namespace Editor::Assets {
	bool ListFilesInDirectory(const std::filesystem::path& directory);
}