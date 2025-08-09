#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "asset_archive.h"
#include "debug.h"
#include <filesystem>

namespace AssetManager {
	bool LoadAssets();
#ifdef EDITOR
	bool LoadAssetsFromDirectory(const std::filesystem::path& directory);
#endif

	bool LoadArchive(const std::filesystem::path& path);
	bool SaveArchive(const std::filesystem::path& path);
	bool RepackArchive();

	u64 CreateAsset(AssetType type, size_t size, const char* path, const char* name);
	template <IsAssetHandle HandleType>
	HandleType CreateAsset(u32 size, const char* path, const char* name) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		const u64 id = CreateAsset(assetType, size, path, name);
		return HandleType{ id };
	}

	void* AddAsset(u64 id, AssetType type, size_t size, const char* path, const char* name, void* data = nullptr);
	bool RemoveAsset(u64 id);

	bool ResizeAsset(u64 id, size_t newSize);

	u64 GetAssetId(const std::filesystem::path& relativePath, AssetType type);
	template <IsAssetHandle HandleType>
	HandleType GetAssetHandle(const std::filesystem::path& relativePath) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		u64 id = GetAssetId(relativePath, assetType);
		return HandleType{ id };
	}
	void* GetAsset(u64 id, AssetType type);
	template <IsAssetHandle HandleType>
	typename AssetHandleTraits<HandleType>::data_type* GetAsset(const HandleType& handle) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		using T = typename AssetHandleTraits<HandleType>::data_type;
		return (T*)GetAsset(handle.id, assetType);
	}

	AssetEntry* GetAssetInfo(u64 id);
	const char* GetAssetName(u64 id);

	void GetAllAssetInfosByType(AssetType type, size_t& count, const AssetEntry** ppOutEntries);

	size_t GetAssetCount();
	const AssetIndex& GetIndex();
}