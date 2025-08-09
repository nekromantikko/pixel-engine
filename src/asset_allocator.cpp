#include "asset_allocator.h"
#include "memory_arena.h"
#include <cstdlib>
#include <cstring>

// Malloc-based allocation functions
static void* MallocAlloc(size_t size, void* userData) {
	(void)userData; // Unused
	return malloc(size);
}

static void* MallocRealloc(void* ptr, size_t oldSize, size_t newSize, void* userData) {
	(void)oldSize; // Standard realloc doesn't need old size
	(void)userData; // Unused
	return realloc(ptr, newSize);
}

static void MallocFree(void* ptr, void* userData) {
	(void)userData; // Unused
	free(ptr);
}

// Arena-based allocation functions  
static void* ArenaAlloc(size_t size, void* userData) {
	Arena* arena = static_cast<Arena*>(userData);
	if (!arena) {
		// Fallback to permanent arena if no specific arena provided
		arena = ArenaAllocator::GetArena(ARENA_PERMANENT);
	}
	return arena ? arena->Push(size) : nullptr;
}

static void* ArenaRealloc(void* ptr, size_t oldSize, size_t newSize, void* userData) {
	Arena* arena = static_cast<Arena*>(userData);
	if (!arena) {
		arena = ArenaAllocator::GetArena(ARENA_PERMANENT);
	}
	
	if (!arena) {
		return nullptr;
	}
	
	if (!ptr) {
		// Act like malloc if ptr is null
		return arena->Push(newSize);
	}
	
	if (newSize == 0) {
		// Act like free if newSize is 0 - but arenas don't free individual blocks
		// Just return null to indicate "freed"
		return nullptr;
	}
	
	if (newSize <= oldSize) {
		// Shrinking - arena can't shrink in place, but we can just return the same pointer
		// The extra space will be wasted but that's the trade-off for arena allocation speed
		return ptr;
	}
	
	// Growing - for simplicity, always allocate new block and copy
	// In the future, we could optimize to check if this is the most recent allocation
	// and try to expand in place, but that adds complexity
	void* newPtr = arena->Push(newSize);
	if (newPtr) {
		memcpy(newPtr, ptr, oldSize);
		// Note: Original memory stays in arena (can't free individual blocks)
		return newPtr;
	}
	
	return nullptr; // Allocation failed
}

static void ArenaFree(void* ptr, void* userData) {
	(void)ptr; // Arena allocators don't free individual blocks
	(void)userData;
	// No-op: arena memory is managed by arena lifecycle
}

// Public API implementation
AssetAllocator AssetAllocators::GetMallocAllocator() {
	return AssetAllocator(MallocAlloc, MallocRealloc, MallocFree, nullptr);
}

AssetAllocator AssetAllocators::GetArenaAllocator() {
	// Use permanent arena as default for asset storage
	Arena* arena = ArenaAllocator::GetArena(ARENA_PERMANENT);
	return AssetAllocator(ArenaAlloc, ArenaRealloc, ArenaFree, arena);
}

AssetAllocator AssetAllocators::GetDefaultAllocator() {
	// Check if arenas are available and initialized
	if (ArenaAllocator::IsInitialized()) {
		return GetArenaAllocator();
	} else {
		return GetMallocAllocator();
	}
}