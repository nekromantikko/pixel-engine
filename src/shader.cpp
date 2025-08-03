#include "shader.h"
#include "debug.h"
#include "asset_manager.h"
#include "rendering_types.h"
#include <cstring>

bool Assets::LoadShaderFromFile(const std::filesystem::path& path, u32& dataSize, void* data) {
    // Extract shader name from path for asset lookup
    std::string shaderName = path.filename().replace_extension("").string();
    
    // Try to find shader in asset manager first
    const AssetIndex& index = AssetManager::GetIndex();
    for (const auto& [id, entry] : index) {
        if (entry.flags.type == ASSET_TYPE_SHADER && 
            std::string(entry.name) == shaderName) {
            
            const Shader* pShader = AssetManager::GetAsset<ShaderHandle>(ShaderHandle{id});
            if (pShader) {
                dataSize = pShader->dataSize;
                if (data) {
                    memcpy(data, pShader->GetData(), dataSize);
                }
                return true;
            }
        }
    }
    
    // Fallback to direct file loading if not found in assets
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