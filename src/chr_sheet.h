#pragma once
#include "rendering.h"
#include <filesystem>

namespace Assets {
	bool LoadChrSheetFromFile(const std::filesystem::path& path, void* pOutData);
}