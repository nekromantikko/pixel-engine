#pragma once
#include "typedef.h"
#include <filesystem>
#include <vector>
#include <string>

namespace ShaderCompiler {
	bool Compile(const char* name, const char* path, const char* source, std::vector<u8>& outData);
	
#ifdef EDITOR
	// Shader caching functions
	std::string CalculateSourceHash(const std::string& source);
	std::filesystem::path GetCacheDirectory();
	std::filesystem::path GetCachedShaderPath(const std::string& hash);
	bool LoadCachedShader(const std::string& hash, std::vector<u8>& outData);
	bool SaveCachedShader(const std::string& hash, const std::vector<u8>& data);
#endif
}