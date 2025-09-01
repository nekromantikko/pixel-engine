#include "shader_compiler.h"
#include "core_types.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>

static Slang::ComPtr<slang::IGlobalSession> g_slangGlobalSession = nullptr;
Slang::ComPtr<slang::ISession> g_slangSession = nullptr;

static bool InitSession() {
	if (g_slangGlobalSession && g_slangSession) {
		return true; // Session already initialized
	}

	slang::createGlobalSession(g_slangGlobalSession.writeRef());

	slang::SessionDesc sessionDesc{};

	slang::TargetDesc targetDesc{};
#ifdef RENDERING_BACKEND_VK
	targetDesc.format = SLANG_SPIRV;
	targetDesc.profile = g_slangGlobalSession->findProfile("spirv_1_5");
#else
    targetDesc.format = SLANG_TARGET_UNKNOWN;
    targetDesc.profile = SLANG_PROFILE_UNKNOWN;
#endif

	sessionDesc.targets = &targetDesc;
	sessionDesc.targetCount = 1;

	std::string viewportWidthPixels = std::to_string(VIEWPORT_WIDTH_PIXELS);
	std::string viewportHeightPixels = std::to_string(VIEWPORT_HEIGHT_PIXELS);
	std::string maxSpritesPerScanline = std::to_string(MAX_SPRITES_PER_SCANLINE);
	std::string nametableSizeTiles = std::to_string(NAMETABLE_SIZE_TILES);
	std::string chrSizeTiles = std::to_string(CHR_SIZE_TILES);
	std::string paletteColorCount = std::to_string(PALETTE_COLOR_COUNT);
	std::string nametableDimPixels = std::to_string(NAMETABLE_DIM_PIXELS);
	std::string tileDimPixels = std::to_string(TILE_DIM_PIXELS);

	slang::PreprocessorMacroDesc preprocessorMacros[] = {
		{ "VIEWPORT_WIDTH_PIXELS", viewportWidthPixels.c_str() },
		{ "VIEWPORT_HEIGHT_PIXELS", viewportHeightPixels.c_str() },
		{ "MAX_SPRITES_PER_SCANLINE", maxSpritesPerScanline.c_str() },
		{ "NAMETABLE_SIZE_TILES", nametableSizeTiles.c_str() },
		{ "CHR_SIZE_TILES", chrSizeTiles.c_str() },
		{ "PALETTE_COLOR_COUNT", paletteColorCount.c_str() },
		{ "NAMETABLE_DIM_PIXELS", nametableDimPixels.c_str() },
		{ "TILE_DIM_PIXELS", tileDimPixels.c_str() },
	};

	sessionDesc.preprocessorMacros = preprocessorMacros;
	sessionDesc.preprocessorMacroCount = 8;

	g_slangGlobalSession->createSession(sessionDesc, g_slangSession.writeRef());

	return true;
}

bool ShaderCompiler::Compile(const char* name, const char* path, const char* source, std::vector<u8>& outData) {
	InitSession();

	slang::IModule *module = g_slangSession->loadModuleFromSourceString(name, path, source);
	size_t entryPointCount = module->getDefinedEntryPointCount();

	if (entryPointCount == 0) {
		return false;
	}

	std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints(entryPointCount);
	for (size_t i = 0; i < entryPointCount; ++i) {
		module->getDefinedEntryPoint(i, entryPoints[i].writeRef());
	}

	std::vector<slang::IComponentType*> componentTypes;
	componentTypes.emplace_back(module);
	for (size_t i = 0; i < entryPointCount; ++i) {
		componentTypes.emplace_back(entryPoints[i]);
	}

	Slang::ComPtr<slang::IComponentType> linkedProgram;
	module->link(linkedProgram.writeRef());

	Slang::ComPtr<slang::IBlob> spirvBlob;
	linkedProgram->getTargetCode(0, spirvBlob.writeRef());

	outData.clear();
	outData.resize(spirvBlob->getBufferSize());
	memcpy(outData.data(), spirvBlob->getBufferPointer(), spirvBlob->getBufferSize());

	return true;
}

std::string ShaderCompiler::CalculateSourceHash(const std::string& source) {
	// Use std::hash for simplicity in editor code - performance not critical
	std::hash<std::string> hasher;
	size_t hashValue = hasher(source);

	// Convert to hex string
	std::stringstream ss;
	ss << std::hex << hashValue;
	return ss.str();
}

std::filesystem::path ShaderCompiler::GetCacheDirectory() {
	std::filesystem::path cacheDir = "shader_cache";

	// Create cache directory if it doesn't exist
	if (!std::filesystem::exists(cacheDir)) {
		std::filesystem::create_directories(cacheDir);
	}

	return cacheDir;
}

std::filesystem::path ShaderCompiler::GetCachedShaderPath(const std::string& hash) {
	return GetCacheDirectory() / (hash + ".spv");
}

bool ShaderCompiler::LoadCachedShader(const std::string& hash, std::vector<u8>& outData) {
	std::filesystem::path cachePath = GetCachedShaderPath(hash);

	if (!std::filesystem::exists(cachePath)) {
		return false;
	}

	try {
		std::ifstream file(cachePath, std::ios::binary);
		if (!file.is_open()) {
			return false;
		}

		// Get file size
		file.seekg(0, std::ios::end);
		size_t size = file.tellg();
		file.seekg(0, std::ios::beg);

		// Read cached data
		outData.resize(size);
		file.read(reinterpret_cast<char*>(outData.data()), size);

		return file.good();
	}
	catch (...) {
		return false;
	}
}

bool ShaderCompiler::SaveCachedShader(const std::string& hash, const std::vector<u8>& data) {
	std::filesystem::path cachePath = GetCachedShaderPath(hash);

	try {
		// Ensure cache directory exists
		std::filesystem::create_directories(cachePath.parent_path());

		std::ofstream file(cachePath, std::ios::binary);
		if (!file.is_open()) {
			return false;
		}

		file.write(reinterpret_cast<const char*>(data.data()), data.size());
		return file.good();
	}
	catch (...) {
		return false;
	}
}