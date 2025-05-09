#include "shader.h"
#include "debug.h"

bool Assets::LoadShaderFromFile(const std::filesystem::path& path, u32& dataSize, void* data) {
    if (!std::filesystem::exists(path)) {
        DEBUG_ERROR("File (%s) does not exist\n", path.string().c_str());
        return false;
    }

    FILE* pFile = fopen(path.string().c_str(), "rb");
    if (!pFile) {
        DEBUG_ERROR("Failed to open file\n");
        return false;
    }

	fseek(pFile, 0, SEEK_END);
	dataSize = ftell(pFile);

    if (data) {
		fseek(pFile, 0, SEEK_SET);
		fread(data, 1, dataSize, pFile);
    }

    fclose(pFile);
    return true;
}