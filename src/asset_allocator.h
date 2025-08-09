#pragma once
#include "typedef.h"

// Forward declarations
class Arena;

// Allocation strategy callbacks for AssetArchive
// 
// This system allows AssetArchive to use different allocation strategies:
// - Arena allocation for engine runtime (fast, no fragmentation)  
// - Malloc allocation for tools like asset packer (standard behavior)
struct AssetAllocator {
	// Function pointers for allocation operations
	void* (*Alloc)(size_t size, void* userData);
	void* (*Realloc)(void* ptr, size_t oldSize, size_t newSize, void* userData);
	void (*Free)(void* ptr, void* userData);
	
	// Optional user data (e.g., arena pointer)
	void* userData;
	
	// Constructor for easy initialization
	AssetAllocator(
		void* (*allocFn)(size_t, void*),
		void* (*reallocFn)(void*, size_t, size_t, void*), 
		void (*freeFn)(void*, void*),
		void* data = nullptr
	) : Alloc(allocFn), Realloc(reallocFn), Free(freeFn), userData(data) {}
	
	AssetAllocator() : Alloc(nullptr), Realloc(nullptr), Free(nullptr), userData(nullptr) {}
};

// Predefined allocator strategies
namespace AssetAllocators {
	// Standard malloc-based allocator (for asset packer)
	AssetAllocator GetMallocAllocator();
	
	// Arena-based allocator (for engine, requires arena system to be initialized)
	AssetAllocator GetArenaAllocator();
	
	// Get the default allocator based on compile-time context
	AssetAllocator GetDefaultAllocator();
}