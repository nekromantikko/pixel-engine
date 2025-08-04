#pragma once
#include "typedef.h"
#include <filesystem>
#include <vector>

namespace ShaderCompiler {
	bool Compile(const char* name, const char* path, const char* source, std::vector<u8>& outData);
}