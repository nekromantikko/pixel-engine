#pragma once
#include "asset_types.h"
#include <filesystem>
#include <map>

namespace Editor::Assets {
	void InitializeAsset(AssetType type, void* pData);
	u32 GetAssetSize(AssetType type, const void* pData);
}