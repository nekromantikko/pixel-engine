#include "asset_archive.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

AssetArchive::AssetArchive() 
	: m_capacity(0), m_size(0), m_data(nullptr) {
}

AssetArchive::~AssetArchive() {
	Clear();
}

constexpr u32 AssetArchive::GetNextPOT(u32 n) {
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

bool AssetArchive::ResizeStorage(u32 minCapacity) {
	const u32 newCapacity = GetNextPOT(minCapacity);
	void* newBlock = realloc(m_data, newCapacity);
	if (!newBlock) {
		return false;
	}

	m_data = (u8*)newBlock;
	m_capacity = newCapacity;
	return true;
}

bool AssetArchive::ReserveMemory(u32 size) {
	const u32 minCapacity = m_size + size;
	if (m_capacity < minCapacity) {
		if (!ResizeStorage(minCapacity)) {
			return false;
		}
	}
	return true;
}

bool AssetArchive::CreateEmpty() {
	Clear();
	return true;
}

bool AssetArchive::LoadFromFile(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		return CreateEmpty();
	}

	// Clear existing data
	Clear();

	FILE* pFile = fopen(path.string().c_str(), "rb");
	if (!pFile) {
		return false;
	}

	ArchiveHeader header;
	fread(&header, sizeof(ArchiveHeader), 1, pFile);

	constexpr char validSignature[4] = {'N','P','A','K'};
	if (memcmp(header.signature, validSignature, 4) != 0) {
		fclose(pFile);
		return false;
	}

	const u32 size = header.directoryOffset - sizeof(ArchiveHeader);
	if (!ResizeStorage(size)) {
		fclose(pFile);
		return false;
	}
	
	fread(m_data, 1, size, pFile);
	m_size = size;

	m_index.reserve(header.assetCount);
	for (u32 i = 0; i < header.assetCount; i++) {
		AssetEntry asset;
		fread(&asset, sizeof(AssetEntry), 1, pFile);
		m_index.emplace(asset.id, asset);
	}

	fclose(pFile);
	return true;
}

bool AssetArchive::SaveToFile(const std::filesystem::path& path) {
	FILE* pFile = fopen(path.string().c_str(), "wb");
	if (!pFile) {
		return false;
	}

	Repack();

	ArchiveHeader header{
		.signature = { 'N','P','A','K' },
		.assetCount = static_cast<u32>(m_index.size()),
		.directoryOffset = sizeof(ArchiveHeader) + m_size,
	};
	fwrite(&header, sizeof(ArchiveHeader), 1, pFile);

	fwrite(m_data, 1, m_size, pFile);

	for (const auto& kvp : m_index) {
		const AssetEntry& asset = kvp.second;
		fwrite(&asset, sizeof(AssetEntry), 1, pFile);
	}

	fclose(pFile);
	return true;
}

void* AssetArchive::AddAsset(u64 id, AssetType type, u32 size, const char* path, const char* name, const void* data) {
	const auto it = m_index.find(id);
	if (it != m_index.end()) {
		return nullptr; // Asset already exists
	}

	if (!ReserveMemory(size)) {
		return nullptr;
	}

	AssetEntry newEntry{};
	newEntry.id = id;
	strncpy(newEntry.name, name, MAX_ASSET_NAME_LENGTH - 1);
	newEntry.name[MAX_ASSET_NAME_LENGTH - 1] = '\0';
	strncpy(newEntry.relativePath, path, MAX_ASSET_PATH_LENGTH - 1);
	newEntry.relativePath[MAX_ASSET_PATH_LENGTH - 1] = '\0';
	newEntry.offset = m_size;
	newEntry.size = size;
	newEntry.flags.type = type;
	newEntry.flags.deleted = false;
	newEntry.flags.compressed = false;

	m_index.emplace(id, newEntry);

	if (data) {
		memcpy(m_data + m_size, data, size);
	} else {
		memset(m_data + m_size, 0, size);
	}
	m_size += size;

	return m_data + newEntry.offset;
}

bool AssetArchive::RemoveAsset(u64 id) {
	const auto it = m_index.find(id);
	if (it == m_index.end()) {
		return false;
	}

	AssetEntry& asset = it->second;
	if (asset.flags.deleted) {
		return false;
	}

	asset.flags.deleted = true;
	return true;
}

bool AssetArchive::ResizeAsset(u64 id, u32 newSize) {
	const auto it = m_index.find(id);
	if (it == m_index.end()) {
		return false;
	}
	AssetEntry& asset = it->second;

	const u32 oldSize = asset.size;

	if (newSize > oldSize) {
		// TODO: This could reserve more space than needed to avoid repeated resizes that fragment the memory
		if (!ReserveMemory(newSize)) {
			return false;
		}

		memcpy(m_data + m_size, m_data + asset.offset, oldSize);
		asset.offset = m_size;
		m_size += newSize;
	}

	asset.size = newSize;
	return true;
}

void* AssetArchive::GetAssetData(u64 id, AssetType type) {
	const auto it = m_index.find(id);
	if (it == m_index.end()) {
		return nullptr;
	}
	const AssetEntry& asset = it->second;
	if (asset.flags.type != type || asset.flags.deleted) {
		return nullptr;
	}

	return m_data + asset.offset;
}

AssetEntry* AssetArchive::GetAssetEntry(u64 id) {
	const auto it = m_index.find(id);
	if (it == m_index.end()) {
		return nullptr;
	}
	return &it->second;
}

void AssetArchive::Repack() {
	u32 newSize = 0;
	for (auto& kvp : m_index) {
		auto& [id, asset] = kvp;
		if (asset.flags.deleted) {
			continue;
		}
		newSize += asset.size;
	}

	u8* newData = (u8*)malloc(newSize);
	if (!newData) {
		return;
	}

	// Remove deleted assets from index
	const u32 removedCount = std::erase_if(m_index, [](const auto& item) {
		const auto& [id, asset] = item;
		return asset.flags.deleted;
	});

	u32 offset = 0;
	for (auto& kvp : m_index) {
		auto& [id, asset] = kvp;
		memcpy(newData + offset, m_data + asset.offset, asset.size);
		asset.offset = offset;
		offset += asset.size;
	}

	free(m_data);
	m_data = newData;
	m_size = newSize;
	m_capacity = newSize;
}

void AssetArchive::Clear() {
	if (m_data) {
		free(m_data);
		m_data = nullptr;
	}
	m_capacity = 0;
	m_size = 0;
	m_index.clear();
}

u32 AssetArchive::GetAssetCount() const {
	return static_cast<u32>(m_index.size());
}

const AssetIndex& AssetArchive::GetIndex() const {
	return m_index;
}