#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "asset_archive.h"
#include "debug.h"
#include <filesystem>

namespace AssetManager {
	bool LoadAssets();

	bool LoadAssetsFromDirectory(const std::filesystem::path& directory);

	bool LoadArchive(const std::filesystem::path& path);
	bool SaveArchive(const std::filesystem::path& path);
	bool RepackArchive();

	u64 CreateAsset(AssetType type, u32 size, const char* path, const char* name);
	template <IsAssetHandle HandleType>
	HandleType CreateAsset(u32 size, const char* path, const char* name) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		const u64 id = CreateAsset(assetType, size, path, name);
		return HandleType{ id };
	}

	void* AddAsset(u64 id, AssetType type, u32 size, const char* path, const char* name, void* data = nullptr);
	bool RemoveAsset(u64 id);

	bool ResizeAsset(u64 id, u32 newSize);

	void* GetAsset(u64 id, AssetType type);
	template <IsAssetHandle HandleType>
	typename AssetHandleTraits<HandleType>::data_type* GetAsset(const HandleType& handle) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		using T = typename AssetHandleTraits<HandleType>::data_type;
		return (T*)GetAsset(handle.id, assetType);
	}

	AssetEntry* GetAssetInfo(u64 id);
	const char* GetAssetName(u64 id);

	u32 GetAssetCount();
	const AssetIndex& GetIndex();
}