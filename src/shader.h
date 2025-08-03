#pragma once
#include "typedef.h"
#include "asset_types.h"
#include <filesystem>

namespace Assets {
	bool LoadShaderFromFile(const std::filesystem::path& path, u32& dataSize, void* data = nullptr);
}