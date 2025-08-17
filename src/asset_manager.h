#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "asset_archive.h"
#include "debug.h"
#include <filesystem>

namespace AssetManager {
	void Free();
	bool LoadArchive(const std::filesystem::path& path);
	bool SaveArchive(const std::filesystem::path& path);
	bool RepackArchive();

	u64 CreateAsset(AssetType type, size_t size, const char* path);
	template <IsAssetHandle HandleType>
	HandleType CreateAsset(u32 size, const char* path) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		const u64 id = CreateAsset(assetType, size, path);
		return HandleType{ id };
	}

	void* AddAsset(u64 id, AssetType type, size_t size, const char* path, void* data = nullptr);
	bool RemoveAsset(u64 id);

	bool ResizeAsset(u64 id, size_t newSize);

	u64 GetAssetIdFromPath(const std::filesystem::path& relativePath);
	u64 GetAssetIdFromPath(const std::filesystem::path& relativePath, AssetType type);
	template <IsAssetHandle HandleType>
	HandleType GetAssetHandleFromPath(const std::filesystem::path& relativePath) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		u64 id = GetAssetIdFromPath(relativePath, assetType);
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
	AssetEntry* GetAssetInfoFromPath(const std::filesystem::path& relativePath);

	void GetAllAssetInfosByType(AssetType type, size_t& count, const AssetEntry** ppOutEntries);

	size_t GetAssetCount();
	const AssetIndex& GetIndex();
}