#pragma once
#include "asset_archive.h"

namespace AssetManager {
	bool Init(AssetArchive* pArchive);

	u64 GetAssetId(const std::filesystem::path& relativePath, AssetType type);
	template <IsAssetHandle HandleType>
	HandleType GetAssetHandle(const std::filesystem::path& relativePath) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		u64 id = GetAssetId(relativePath, assetType);
		return HandleType{ id };
	}
	const void* GetAsset(u64 id, AssetType type);
	template <IsAssetHandle HandleType>
	const typename AssetHandleTraits<HandleType>::data_type* GetAsset(const HandleType& handle) {
		constexpr AssetType assetType = AssetHandleTraits<HandleType>::asset_type::value;
		using T = typename AssetHandleTraits<HandleType>::data_type;
		return (T*)GetAsset(handle.id, assetType);
	}
}