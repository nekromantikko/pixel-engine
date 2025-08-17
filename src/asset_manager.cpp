#include "asset_manager.h"
#include "random.h"
#include <cstdio>
#include <cstring>

static AssetArchive g_archive;

#pragma region Public API
void AssetManager::Free() {
	g_archive.Clear();
}

bool AssetManager::LoadArchive(const std::filesystem::path& path) {
	return g_archive.LoadFromFile(path);
}

bool AssetManager::SaveArchive(const std::filesystem::path& path) {
	return g_archive.SaveToFile(path);
}

bool AssetManager::RepackArchive() {
	g_archive.Repack();
	return true;
}

u64 AssetManager::CreateAsset(AssetType type, size_t size, const char* path) {
	DEBUG_LOG("Creating new asset of size %d with path %s\n", size, path);

	const u64 id = Random::GenerateUUID();
	void* data = g_archive.AddAsset(id, type, size, path, nullptr);
	if (!data) {
		return UUID_NULL;
	}

	return id;
}

void* AssetManager::AddAsset(u64 id, AssetType type, size_t size, const char* path, void* data) {
	return g_archive.AddAsset(id, type, size, path, data);
}

bool AssetManager::RemoveAsset(u64 id) {
	if (!g_archive.RemoveAsset(id)) {
		DEBUG_ERROR("Asset with ID %llu does not exist\n", id);
		return false;
	}

	DEBUG_LOG("Removing asset %llu\n", id);
	return true;
}

bool AssetManager::ResizeAsset(u64 id, size_t newSize) {
	AssetEntry* asset = g_archive.GetAssetEntry(id);
	if (!asset) {
		return false;
	}
	
	const size_t oldSize = asset->size;
	DEBUG_LOG("Resizing asset %lld (%d -> %d)\n", id, oldSize, newSize);

	return g_archive.ResizeAsset(id, newSize);
}

u64 AssetManager::GetAssetId(const std::filesystem::path& relativePath, AssetType type) {
	AssetEntry* pAssetInfo = g_archive.GetAssetEntryByPath(relativePath);
	if (!pAssetInfo) {
		DEBUG_ERROR("Asset with path '%s' not found\n", relativePath.string().c_str());
		return UUID_NULL;
	}
	if (pAssetInfo->flags.type != type) {
		//DEBUG_ERROR("Asset with path '%s' is not of type %d\n", relativePath.string().c_str(), type);
		return UUID_NULL;
	}
	return pAssetInfo->id;
}

void* AssetManager::GetAsset(u64 id, AssetType type) {
	return g_archive.GetAssetData(id, type);
}

AssetEntry* AssetManager::GetAssetInfo(u64 id) {
	return g_archive.GetAssetEntry(id);
}

// If ppOutEntries is nullptr, returns the count of assets of that type
void AssetManager::GetAllAssetInfosByType(AssetType type, size_t& count, const AssetEntry** ppOutEntries) {
	const AssetIndex& index = g_archive.GetIndex();
	count = 0;
	for (size_t i = 0; i < index.Count(); ++i) {
		const AssetEntry* pEntry = index.Get(index.GetHandle(i));
		if (pEntry && pEntry->flags.type == type) {
			if (ppOutEntries) {
				ppOutEntries[count] = pEntry;
			}
			count++;
		}
	}
}

size_t AssetManager::GetAssetCount() {
	return g_archive.GetAssetCount();
}

const AssetIndex& AssetManager::GetIndex() {
	return g_archive.GetIndex();
}
#pragma endregion