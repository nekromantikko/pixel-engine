#include "asset_manager.h"
#include "asset_serialization.h"
#include "random.h"
#include <cstdio>
#include <cstring>

static AssetArchive g_archive;

#pragma region Public API
bool AssetManager::LoadAssets() {
#ifdef ASSETS_NPAK_OUTPUT
	return LoadArchive(ASSETS_NPAK_OUTPUT);
#else
	// For tests without defined macro, just return true
	return true;
#endif
}

#ifdef EDITOR
bool AssetManager::LoadAssetsFromDirectory(const std::filesystem::path& directory) {
	if (!std::filesystem::exists(directory)) {
		DEBUG_ERROR("Directory (%s) does not exist\n", directory.string().c_str());
		return false;
	}

	if (!std::filesystem::is_directory(directory)) {
		DEBUG_ERROR("Path (%s) is not a directory\n", directory.string().c_str());
		return false;
	}

	DEBUG_LOG("Listing assets in directory: %s\n", directory.string().c_str());

	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
		AssetType assetType;
		if (entry.is_regular_file() && Serialization::TryGetAssetTypeFromPath(entry.path(), assetType)) {
			const std::string pathStr = entry.path().string();
			const char* pathCStr = pathStr.c_str();
			DEBUG_LOG("Found %s: %s\n", ASSET_TYPE_NAMES[assetType], pathCStr);

			nlohmann::json metadata;
			if (!Serialization::TryGetAssetMetadata(entry.path(), metadata)) {
				DEBUG_ERROR("Failed to load metadata for asset %s\n", pathCStr);
				continue;
			}

			const u64 guid = metadata["guid"];

			u32 size;
			if (!Serialization::LoadAssetFromFile(entry.path(), assetType, metadata, size, nullptr)) {
				DEBUG_ERROR("Failed to get size for asset %s\n", pathCStr);
				continue;
			}
			const std::string filenameWithoutExt = entry.path().filename().replace_extension("").string();
			std::string name = filenameWithoutExt;
			if (metadata.contains("name") && !metadata["name"].is_null())
			{
				name = metadata["name"].get<std::string>();
			}

			const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), ASSETS_SRC_DIR);
			void* pData = AssetManager::AddAsset(guid, assetType, size, relativePath.string().c_str(), name.c_str(), nullptr);
			if (!pData) {
				DEBUG_ERROR("Failed to add asset %s to manager\n", pathCStr);
				continue;
			}
			if (!Serialization::LoadAssetFromFile(entry.path(), assetType, metadata, size, pData)) {
				DEBUG_ERROR("Failed to load asset data from %s\n", pathCStr);
				AssetManager::RemoveAsset(guid);
				continue;
			}
			DEBUG_LOG("Asset %s loaded successfully with GUID: %llu\n", pathCStr, guid);
		}
	}

	return true;
}
#endif

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

u64 AssetManager::CreateAsset(AssetType type, u32 size, const char* path, const char* name) {
	DEBUG_LOG("Creating new asset of size %d with name %s\n", size, name);

	const u64 id = Random::GenerateUUID();
	void* data = g_archive.AddAsset(id, type, size, path, name, nullptr);
	if (!data) {
		return UUID_NULL;
	}

	return id;
}

void* AssetManager::AddAsset(u64 id, AssetType type, u32 size, const char* path, const char* name, void* data) {
	return g_archive.AddAsset(id, type, size, path, name, data);
}

bool AssetManager::RemoveAsset(u64 id) {
	if (!g_archive.RemoveAsset(id)) {
		DEBUG_ERROR("Asset with ID %llu does not exist\n", id);
		return false;
	}

	DEBUG_LOG("Removing asset %llu\n", id);
	return true;
}

bool AssetManager::ResizeAsset(u64 id, u32 newSize) {
	AssetEntry* asset = g_archive.GetAssetEntry(id);
	if (!asset) {
		return false;
	}
	
	const u32 oldSize = asset->size;
	DEBUG_LOG("Resizing asset %lld (%d -> %d)\n", id, oldSize, newSize);

	return g_archive.ResizeAsset(id, newSize);
}

void* AssetManager::GetAsset(u64 id, AssetType type) {
	return g_archive.GetAssetData(id, type);
}

AssetEntry* AssetManager::GetAssetInfo(u64 id) {
	return g_archive.GetAssetEntry(id);
}

const char* AssetManager::GetAssetName(u64 id) {
	const AssetEntry* pAssetInfo = GetAssetInfo(id);
	if (!pAssetInfo) {
		return nullptr;
	}
	return pAssetInfo->name;
}

u32 AssetManager::GetAssetCount() {
	return g_archive.GetAssetCount();
}

const AssetIndex& AssetManager::GetIndex() {
	return g_archive.GetIndex();
}
#pragma endregion