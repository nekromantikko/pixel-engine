#include "asset_manager.h"
#include "fixed_hash_map.h"
#include <cstdio>

struct ArchiveHeader {
	char signature[4];
	u32 assetCount;
	u32 directoryOffset;
};

static u32 archiveCapacity = 0;
static u32 archiveSize = 0;
static u8* archiveData = nullptr;

static AssetIndex assetIndex;

static constexpr u32 GetNextPOT(u32 n) {
	if (n == 0) {
		return 1;
	}

	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

static bool ResizeArchive(u32 minCapacity) {
	const u32 newCapacity = GetNextPOT(minCapacity);
	void* newBlock = realloc(archiveData, newCapacity);
	if (!newBlock) {
		return false;
	}

	DEBUG_LOG("Resizing asset archive (%d -> %d)\n", archiveCapacity, newCapacity);
	archiveData = (u8*)newBlock;
	archiveCapacity = newCapacity;
	return true;
}

static bool CreateEmptyArchive(const std::filesystem::path& path) {
	FILE* pFile = fopen(path.string().c_str(), "wb");
	if (!pFile) {
		DEBUG_ERROR("Failed to create asset archive\n");
		return false;
	}

	ArchiveHeader header{
		.signature = {'N','P','A','K'},
		.assetCount = 0,
		.directoryOffset = sizeof(ArchiveHeader)
	};
	fwrite(&header, sizeof(ArchiveHeader), 1, pFile);
	fclose(pFile);
	return true;
}

static bool ReserveMemory(u32 size) {
	const u32 minCapacity = archiveSize + size;
	if (archiveCapacity < minCapacity) {
		if (!ResizeArchive(minCapacity)) {
			return false;
		}
	}

	return true;
}

#pragma region Public API
bool AssetManager::LoadArchive(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		return CreateEmptyArchive(path);
	}

	// Clear existing data
	assetIndex.clear();
	archiveSize = 0;

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		DEBUG_ERROR("Failed to load asset archive\n");
		return false;
	}

	ArchiveHeader header;
	fread(&header, sizeof(ArchiveHeader), 1, pFile);

	constexpr char validSignature[4] = {'N','P','A','K'};
	if (memcmp(header.signature, validSignature, 4) != 0) {
		DEBUG_ERROR("Invalid asset archive file\n");
		return false;
	}

	const u32 size = header.directoryOffset - sizeof(ArchiveHeader);
	ResizeArchive(size);
	fread(archiveData, 1, size, pFile);
	archiveSize = size;

	assetIndex.reserve(header.assetCount);
	for (u32 i = 0; i < header.assetCount; i++) {
		AssetEntry asset;
		fread(&asset, sizeof(AssetEntry), 1, pFile);

		assetIndex.emplace(asset.id, asset);
	}

	fclose(pFile);
	return true;
}

bool AssetManager::SaveArchive(const std::filesystem::path& path) {
	FILE* pFile = fopen(path.string().c_str(), "wb");
	if (!pFile) {
		DEBUG_ERROR("Failed to save asset archive\n");
		return false;
	}

	RepackArchive();

	ArchiveHeader header{
		.signature = { 'N','P','A','K' },
		.assetCount = u32(assetIndex.size()),
		.directoryOffset = sizeof(ArchiveHeader) + archiveSize,
	};
	fwrite(&header, sizeof(ArchiveHeader), 1, pFile);

	fwrite(archiveData, 1, archiveSize, pFile);

	for (const auto& kvp : assetIndex) {
		const AssetEntry& asset = kvp.second;
		fwrite(&asset, sizeof(AssetEntry), 1, pFile);
	}

	fclose(pFile);
	return true;
}

bool AssetManager::RepackArchive() {
	u32 newSize = 0;
	for (auto& kvp : assetIndex) {
		auto& [id, asset] = kvp;
		if (asset.flags.deleted) {
			continue;
		}

		newSize += asset.size;
	}

	u8* newData = (u8*)malloc(newSize);
	if (!newData) {
		return false;
	}

	const u32 removedCount = std::erase_if(assetIndex, [](const auto& item) {
		const auto& [id, asset] = item;
		return asset.flags.deleted;
		});
	DEBUG_LOG("Removed %d assets\n", removedCount);

	u32 offset = 0;
	for (auto& kvp : assetIndex) {
		auto& [id, asset] = kvp;
		memcpy(newData + offset, archiveData + asset.offset, asset.size);
		asset.offset = offset;
		offset += asset.size;
	}

	archiveSize = newSize;
	memcpy(archiveData, newData, newSize);

	free(newData);
	return true;
}

u64 AssetManager::CreateAsset(AssetType type, u32 size, const char* name) {
	DEBUG_LOG("Creating new asset of size %d with name %s\n", size, name);

	if (!ReserveMemory(size)) {
		return UUID_NULL;
	}

	const u64 id = Random::GenerateUUID();

	AssetEntry newEntry{};
	newEntry.id = id;
	strcpy(newEntry.name, name);
	newEntry.offset = archiveSize;
	newEntry.size = size;
	newEntry.flags.type = type;

	assetIndex.emplace(id, newEntry);

	memset(archiveData + archiveSize, 0, size);
	archiveSize += size;

	return id;
}

bool AssetManager::ResizeAsset(u64 id, u32 newSize) {
	const auto it = assetIndex.find(id);
	if (it == assetIndex.end()) {
		return false;
	}
	AssetEntry& asset = it->second;

	const u32 oldSize = asset.size;
	DEBUG_LOG("Resizing asset %lld (%d -> %d)\n", id, oldSize, newSize);

	if (newSize > oldSize) {
		// TODO: This could reserve more space than needed to avoid repeated resizes that fragment the memory
		if (!ReserveMemory(newSize)) {
			return false;
		}

		memcpy(archiveData + archiveSize, archiveData + asset.offset, oldSize);
		asset.offset = archiveSize;
	}

	asset.size = newSize;
}

void* AssetManager::GetAsset(u64 id, AssetType type) {
	const auto it = assetIndex.find(id);
	if (it == assetIndex.end()) {
		return nullptr;
	}
	const AssetEntry& asset = it->second;
	if (asset.flags.type != type) {
		return nullptr;
	}

	return archiveData + asset.offset;
}

AssetEntry* AssetManager::GetAssetInfo(u64 id) {
	const auto it = assetIndex.find(id);
	if (it == assetIndex.end()) {
		return nullptr;
	}
	return &it->second;
}

const char* AssetManager::GetAssetName(u64 id) {
	const AssetEntry* pAssetInfo = GetAssetInfo(id);
	if (!pAssetInfo) {
		return nullptr;
	}
	return pAssetInfo->name;
}

u32 AssetManager::GetAssetCount() {
	return assetIndex.size();
}

AssetIndex& AssetManager::GetIndex() {
	return assetIndex;
}
#pragma endregion