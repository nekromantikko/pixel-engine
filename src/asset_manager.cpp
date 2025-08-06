#include "asset_manager.h"
#include "debug.h"

static AssetArchive* g_pArchive;

#pragma region Public API
bool AssetManager::Init(AssetArchive* pArchive) {
	g_pArchive = pArchive;
	return g_pArchive != nullptr;
}

u64 AssetManager::GetAssetId(const std::filesystem::path& relativePath, AssetType type) {
	const AssetEntry* pAssetInfo = g_pArchive->GetAssetEntryByPath(relativePath);
	if (!pAssetInfo) {
		DEBUG_ERROR("Asset with path '%s' not found\n", relativePath.string().c_str());
		return UUID_NULL;
	}
	if (pAssetInfo->flags.type != type) {
		DEBUG_ERROR("Asset with path '%s' is not of type %d\n", relativePath.string().c_str(), type);
		return UUID_NULL;
	}
	return pAssetInfo->id;
}

const void* AssetManager::GetAsset(u64 id, AssetType type) {
	const AssetEntry* pAssetInfo = g_pArchive->GetAssetEntry(id);
	if (!pAssetInfo) {
		DEBUG_ERROR("Asset %llu not found\n", id);
		return nullptr;
	}
	if (pAssetInfo->flags.type != type) {
		DEBUG_ERROR("Asset %llu is not of type %d\n", id, type);
		return nullptr;
	}
	return g_pArchive->GetAssetData(pAssetInfo);
}
#pragma endregion