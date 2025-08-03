#include "asset_archive.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

// AssetIndex implementation - simple sorted array (Doom WAD style)
AssetIndex::AssetIndex() : m_entries(nullptr), m_count(0), m_capacity(0) {
}

AssetIndex::~AssetIndex() {
	clear();
}

s32 AssetIndex::FindIndex(u64 id) const {
	if (m_count == 0) {
		return -1;
	}

	// Binary search for the asset ID
	s32 left = 0;
	s32 right = static_cast<s32>(m_count) - 1;
	
	while (left <= right) {
		s32 mid = left + (right - left) / 2;
		u64 midId = m_entries[mid].id;
		
		if (midId == id) {
			return mid;
		} else if (midId < id) {
			left = mid + 1;
		} else {
			right = mid - 1;
		}
	}
	
	return -1; // Not found
}

bool AssetIndex::ResizeIfNeeded() {
	if (m_count >= m_capacity) {
		u32 newCapacity = m_capacity == 0 ? 8 : m_capacity * 2;
		AssetEntry* newEntries = (AssetEntry*)realloc(m_entries, newCapacity * sizeof(AssetEntry));
		if (!newEntries) {
			return false;
		}
		m_entries = newEntries;
		m_capacity = newCapacity;
	}
	return true;
}

AssetEntry* AssetIndex::find(u64 id) {
	s32 index = FindIndex(id);
	return index >= 0 ? &m_entries[index] : nullptr;
}

const AssetEntry* AssetIndex::find(u64 id) const {
	s32 index = FindIndex(id);
	return index >= 0 ? &m_entries[index] : nullptr;
}

bool AssetIndex::emplace(u64 id, const AssetEntry& entry) {
	// Check if asset already exists
	if (find(id) != nullptr) {
		return false;
	}

	if (!ResizeIfNeeded()) {
		return false;
	}

	// Find insertion point to maintain sorted order
	u32 insertPos = 0;
	for (u32 i = 0; i < m_count; i++) {
		if (m_entries[i].id > id) {
			insertPos = i;
			break;
		}
		insertPos = i + 1;
	}

	// Shift elements to the right to make space
	if (insertPos < m_count) {
		memmove(&m_entries[insertPos + 1], &m_entries[insertPos], 
		        (m_count - insertPos) * sizeof(AssetEntry));
	}

	// Insert the new entry
	m_entries[insertPos] = entry;
	m_count++;
	
	return true;
}

bool AssetIndex::erase(u64 id) {
	s32 index = FindIndex(id);
	if (index < 0) {
		return false;
	}

	// Shift elements to the left to fill the gap
	if (static_cast<u32>(index) < m_count - 1) {
		memmove(&m_entries[index], &m_entries[index + 1], 
		        (m_count - static_cast<u32>(index) - 1) * sizeof(AssetEntry));
	}
	
	m_count--;
	return true;
}

void AssetIndex::clear() {
	if (m_entries) {
		free(m_entries);
		m_entries = nullptr;
	}
	m_count = 0;
	m_capacity = 0;
}

void AssetIndex::reserve(u32 capacity) {
	if (capacity > m_capacity) {
		AssetEntry* newEntries = (AssetEntry*)realloc(m_entries, capacity * sizeof(AssetEntry));
		if (newEntries) {
			m_entries = newEntries;
			m_capacity = capacity;
		}
	}
}

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
		.directoryOffset = static_cast<u32>(sizeof(ArchiveHeader) + m_size),
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
	const AssetEntry* existing = m_index.find(id);
	if (existing != nullptr) {
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
	AssetEntry* asset = m_index.find(id);
	if (asset == nullptr) {
		return false;
	}

	if (asset->flags.deleted) {
		return false;
	}

	asset->flags.deleted = true;
	return true;
}

bool AssetArchive::ResizeAsset(u64 id, u32 newSize) {
	AssetEntry* asset = m_index.find(id);
	if (asset == nullptr) {
		return false;
	}

	const u32 oldSize = asset->size;

	if (newSize > oldSize) {
		// TODO: This could reserve more space than needed to avoid repeated resizes that fragment the memory
		if (!ReserveMemory(newSize)) {
			return false;
		}

		memcpy(m_data + m_size, m_data + asset->offset, oldSize);
		asset->offset = m_size;
		m_size += newSize;
	}

	asset->size = newSize;
	return true;
}

void* AssetArchive::GetAssetData(u64 id, AssetType type) {
	const AssetEntry* asset = m_index.find(id);
	if (asset == nullptr) {
		return nullptr;
	}
	if (asset->flags.type != type || asset->flags.deleted) {
		return nullptr;
	}

	return m_data + asset->offset;
}

AssetEntry* AssetArchive::GetAssetEntry(u64 id) {
	return m_index.find(id);
}

void AssetArchive::Repack() {
	u32 newSize = 0;
	for (const auto& kvp : m_index) {
		const AssetEntry& asset = kvp.second;
		if (asset.flags.deleted) {
			continue;
		}
		newSize += asset.size;
	}

	u8* newData = (u8*)malloc(newSize);
	if (!newData) {
		return;
	}

	// Create a temporary list of non-deleted assets with updated offsets
	AssetEntry* tempAssets = (AssetEntry*)malloc(m_index.size() * sizeof(AssetEntry));
	if (!tempAssets) {
		free(newData);
		return;
	}

	u32 tempCount = 0;
	u32 offset = 0;
	for (const auto& kvp : m_index) {
		const AssetEntry& asset = kvp.second;
		if (asset.flags.deleted) {
			continue;
		}
		// Copy asset data to new buffer
		memcpy(newData + offset, m_data + asset.offset, asset.size);
		
		// Create updated asset entry
		tempAssets[tempCount] = asset;
		tempAssets[tempCount].offset = offset;
		
		offset += asset.size;
		tempCount++;
	}

	// Clear old index and rebuild with non-deleted assets
	m_index.clear();
	m_index.reserve(tempCount);
	for (u32 i = 0; i < tempCount; i++) {
		m_index.emplace(tempAssets[i].id, tempAssets[i]);
	}

	// Replace old data
	free(m_data);
	free(tempAssets);
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