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

constexpr size_t AssetArchive::GetNextPOT(size_t n) {
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

bool AssetArchive::ResizeStorage(size_t minCapacity) {
	const size_t newCapacity = GetNextPOT(minCapacity);
	void* newBlock = realloc(m_data, newCapacity);
	if (!newBlock) {
		return false;
	}

	m_data = (u8*)newBlock;
	m_capacity = newCapacity;
	return true;
}

bool AssetArchive::ReserveMemory(size_t size) {
	const size_t minCapacity = m_size + size;
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

	const size_t size = header.directoryOffset - sizeof(ArchiveHeader);
	if (!ResizeStorage(size)) {
		fclose(pFile);
		return false;
	}
	
	fread(m_data, 1, size, pFile);
	m_size = size;

	// Load assets and maintain sorted order by ID
	AssetEntry tempAssets[MAX_ASSETS];
	size_t assetCount = header.assetCount;
	if (assetCount > MAX_ASSETS) {
		fclose(pFile);
		return false;
	}
	
	for (size_t i = 0; i < assetCount; i++) {
		fread(&tempAssets[i], sizeof(AssetEntry), 1, pFile);
	}
	
	// Sort assets by ID
	std::sort(tempAssets, tempAssets + assetCount, [](const AssetEntry& a, const AssetEntry& b) {
		return a.id < b.id;
	});
	
	// Add sorted assets to the pool
	for (size_t i = 0; i < assetCount; i++) {
		m_index.Add(tempAssets[i]);
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
		.assetCount = static_cast<u32>(m_index.Count()),
		.directoryOffset = sizeof(ArchiveHeader) + m_size,
	};
	fwrite(&header, sizeof(ArchiveHeader), 1, pFile);

	fwrite(m_data, 1, m_size, pFile);

	for (u32 i = 0; i < m_index.Count(); i++) {
		PoolHandle<AssetEntry> handle = m_index.GetHandle(i);
		const AssetEntry* asset = m_index.Get(handle);
		if (asset) {
			fwrite(asset, sizeof(AssetEntry), 1, pFile);
		}
	}

	fclose(pFile);
	return true;
}

void* AssetArchive::AddAsset(u64 id, AssetType type, size_t size, const char* path, const char* name, const void* data) {
	const AssetEntry* existing = FindAssetByIdBinary(id);
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

	InsertAssetSorted(newEntry);

	if (data) {
		memcpy(m_data + m_size, data, size);
	} else {
		memset(m_data + m_size, 0, size);
	}
	m_size += size;

	return m_data + newEntry.offset;
}

bool AssetArchive::RemoveAsset(u64 id) {
	AssetEntry* asset = FindAssetByIdBinary(id);
	if (asset == nullptr) {
		return false;
	}

	if (asset->flags.deleted) {
		return false;
	}

	asset->flags.deleted = true;
	return true;
}

bool AssetArchive::ResizeAsset(u64 id, size_t newSize) {
	AssetEntry* asset = FindAssetByIdBinary(id);
	if (asset == nullptr) {
		return false;
	}

	const size_t oldSize = asset->size;

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
	const AssetEntry* asset = FindAssetByIdBinary(id);
	if (asset == nullptr) {
		return nullptr;
	}
	if (asset->flags.type != type || asset->flags.deleted) {
		return nullptr;
	}

	return m_data + asset->offset;
}

AssetEntry* AssetArchive::GetAssetEntry(u64 id) {
	return FindAssetByIdBinary(id);
}

void AssetArchive::Repack() {
	size_t newSize = 0;
	for (u32 i = 0; i < m_index.Count(); i++) {
		PoolHandle<AssetEntry> handle = m_index.GetHandle(i);
		const AssetEntry* asset = m_index.Get(handle);
		if (asset && !asset->flags.deleted) {
			newSize += asset->size;
		}
	}

	u8* newData = (u8*)malloc(newSize);
	if (!newData) {
		return;
	}

	// Remove deleted assets from index and compact remaining ones
	AssetEntry tempAssets[MAX_ASSETS];
	u32 activeCount = 0;
	
	size_t offset = 0;
	for (u32 i = 0; i < m_index.Count(); i++) {
		PoolHandle<AssetEntry> handle = m_index.GetHandle(i);
		AssetEntry* asset = m_index.Get(handle);
		if (asset && !asset->flags.deleted) {
			memcpy(newData + offset, m_data + asset->offset, asset->size);
			asset->offset = offset;
			offset += asset->size;
			tempAssets[activeCount++] = *asset;
		}
	}
	
	// Rebuild the index with only non-deleted assets
	m_index.Clear();
	for (u32 i = 0; i < activeCount; i++) {
		m_index.Add(tempAssets[i]);
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
	m_index.Clear();
}

size_t AssetArchive::GetAssetCount() const {
	return m_index.Count();
}

const AssetIndex& AssetArchive::GetIndex() const {
	return m_index;
}

// Binary search helpers for sorted asset pool
AssetEntry* AssetArchive::FindAssetByIdBinary(u64 id) {
	u32 left = 0;
	u32 right = m_index.Count();
	
	while (left < right) {
		u32 mid = left + (right - left) / 2;
		PoolHandle<AssetEntry> handle = m_index.GetHandle(mid);
		const AssetEntry* entry = m_index.Get(handle);
		
		if (!entry) {
			return nullptr;
		}
		
		if (entry->id == id) {
			return const_cast<AssetEntry*>(entry);
		} else if (entry->id < id) {
			left = mid + 1;
		} else {
			right = mid;
		}
	}
	
	return nullptr;
}

const AssetEntry* AssetArchive::FindAssetByIdBinary(u64 id) const {
	u32 left = 0;
	u32 right = m_index.Count();
	
	while (left < right) {
		u32 mid = left + (right - left) / 2;
		PoolHandle<AssetEntry> handle = m_index.GetHandle(mid);
		const AssetEntry* entry = m_index.Get(handle);
		
		if (!entry) {
			return nullptr;
		}
		
		if (entry->id == id) {
			return entry;
		} else if (entry->id < id) {
			left = mid + 1;
		} else {
			right = mid;
		}
	}
	
	return nullptr;
}

u32 AssetArchive::FindInsertionPointBinary(u64 id) const {
	u32 left = 0;
	u32 right = m_index.Count();
	
	while (left < right) {
		u32 mid = left + (right - left) / 2;
		PoolHandle<AssetEntry> handle = m_index.GetHandle(mid);
		const AssetEntry* entry = m_index.Get(handle);
		
		if (!entry) {
			return left;
		}
		
		if (entry->id < id) {
			left = mid + 1;
		} else {
			right = mid;
		}
	}
	
	return left;
}

void AssetArchive::InsertAssetSorted(const AssetEntry& entry) {
	// Add the entry to the pool
	PoolHandle<AssetEntry> newHandle = m_index.Add(entry);
	if (newHandle == PoolHandle<AssetEntry>::Null()) {
		return; // Pool is full
	}
	
	// Sort the pool to maintain order by asset ID
	// Create a temporary array to sort
	AssetEntry tempEntries[MAX_ASSETS];
	u32 count = m_index.Count();
	
	// Copy all entries
	for (u32 i = 0; i < count; i++) {
		PoolHandle<AssetEntry> handle = m_index.GetHandle(i);
		const AssetEntry* existing = m_index.Get(handle);
		if (existing) {
			tempEntries[i] = *existing;
		}
	}
	
	// Sort by asset ID
	std::sort(tempEntries, tempEntries + count, [](const AssetEntry& a, const AssetEntry& b) {
		return a.id < b.id;
	});
	
	// Rebuild the pool with sorted entries
	m_index.Clear();
	for (u32 i = 0; i < count; i++) {
		m_index.Add(tempEntries[i]);
	}
}