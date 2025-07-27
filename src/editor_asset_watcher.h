#pragma once
#include <filesystem>

namespace Editor {
	bool ListFilesInDirectory(const std::filesystem::path& directory);
}