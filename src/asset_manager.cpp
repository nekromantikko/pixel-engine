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

	// TODO: Repack!

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

u64 AssetManager::CreateAsset(u8 type, u32 size, const char* name) {
	const u32 minCapacity = archiveSize + size;
	if (archiveCapacity < minCapacity) {
		if (!ResizeArchive(minCapacity)) {
			return UUID_NULL;
		}
	}

	archiveSize += size;

	const u64 id = Random::GenerateUUID();

	AssetEntry newEntry{};
	newEntry.id = id;
	strcpy(newEntry.name, name);
	newEntry.offset = archiveSize;
	newEntry.size = size;
	newEntry.flags.type = type;

	assetIndex.emplace(id, newEntry);

	return id;
}

void* AssetManager::GetAsset(u64 id) {
	const auto it = assetIndex.find(id);
	if (it == assetIndex.end()) {
		return nullptr;
	}
	return &it->second;
}

u32 AssetManager::GetAssetCount() {
	return assetIndex.size();;
}

const AssetIndex& AssetManager::GetIndex() {
	return assetIndex;
}
#pragma endregion