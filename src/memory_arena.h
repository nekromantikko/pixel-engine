#pragma once
#include "typedef.h"
#include <cassert>

// Forward declarations
struct MemoryArena;

// Arena marker for stack-based allocation/deallocation
struct ArenaMarker {
    u8* position;
    MemoryArena* arena;
    
    ArenaMarker() : position(nullptr), arena(nullptr) {}
    ArenaMarker(u8* pos, MemoryArena* a) : position(pos), arena(a) {}
};

// Linear memory arena for stack-based allocation
struct MemoryArena {
    u8* base;           // Start of arena memory
    u8* current;        // Current allocation position
    u8* end;            // End of arena memory
    size_t size;        // Total arena size
    size_t used;        // Currently used bytes
    const char* name;   // Arena name for debugging
    
    MemoryArena() : base(nullptr), current(nullptr), end(nullptr), size(0), used(0), name("Unnamed") {}
    
    // Initialize arena with pre-allocated memory
    void Initialize(void* memory, size_t totalSize, const char* arenaName = "Arena") {
        base = static_cast<u8*>(memory);
        current = base;
        end = base + totalSize;
        size = totalSize;
        used = 0;
        name = arenaName;
    }
    
    // Allocate aligned memory from arena
    void* Allocate(size_t bytes, size_t alignment = sizeof(void*)) {
        // Align current pointer
        uintptr_t aligned = (reinterpret_cast<uintptr_t>(current) + alignment - 1) & ~(alignment - 1);
        u8* alignedPtr = reinterpret_cast<u8*>(aligned);
        
        // Check if we have enough space
        if (alignedPtr + bytes > end) {
            assert(false && "Arena out of memory!");
            return nullptr;
        }
        
        void* result = alignedPtr;
        current = alignedPtr + bytes;
        used = current - base;
        
        return result;
    }
    
    // Get current position marker
    ArenaMarker GetMarker() {
        return ArenaMarker(current, this);
    }
    
    // Reset to marker position (stack-style deallocation)
    void ResetToMarker(const ArenaMarker& marker) {
        assert(marker.arena == this);
        assert(marker.position >= base && marker.position <= current);
        
        current = marker.position;
        used = current - base;
    }
    
    // Clear entire arena
    void Clear() {
        current = base;
        used = 0;
    }
    
    // Get remaining bytes
    size_t GetRemainingBytes() const {
        return end - current;
    }
    
    // Get usage statistics
    r32 GetUsagePercent() const {
        return size > 0 ? (r32(used) / r32(size)) * 100.0f : 0.0f;
    }
};

// Convenience macros for arena allocation
#define ARENA_ALLOC(arena, type) static_cast<type*>((arena)->Allocate(sizeof(type), alignof(type)))
#define ARENA_ALLOC_ARRAY(arena, type, count) static_cast<type*>((arena)->Allocate(sizeof(type) * (count), alignof(type)))

// Global arena manager
namespace ArenaAllocator {
    // Arena types
    enum ArenaType {
        ARENA_PERMANENT,    // Long-lived allocations (assets, game data)
        ARENA_TEMPORARY,    // Short-lived allocations (per-frame, temporary calculations)
        ARENA_VULKAN,       // Vulkan-specific allocations
        ARENA_COUNT
    };
    
    // Initialize global arena system
    void Initialize();
    
    // Shutdown and cleanup
    void Shutdown();
    
    // Get arena by type
    MemoryArena* GetArena(ArenaType type);
    
    // Convenience allocation functions
    void* Allocate(ArenaType type, size_t bytes, size_t alignment = sizeof(void*));
    template<typename T>
    T* Allocate(ArenaType type) {
        return static_cast<T*>(Allocate(type, sizeof(T), alignof(T)));
    }
    template<typename T>
    T* AllocateArray(ArenaType type, size_t count) {
        return static_cast<T*>(Allocate(type, sizeof(T) * count, alignof(T)));
    }
    
    // Get arena marker for stack-style allocation
    ArenaMarker GetMarker(ArenaType type);
    
    // Reset arena to marker
    void ResetToMarker(const ArenaMarker& marker);
    
    // Clear arena completely
    void ClearArena(ArenaType type);
    
    // Get memory usage statistics
    void GetMemoryStats(ArenaType type, size_t& used, size_t& total, r32& percent);
    
    // Debug: Print all arena statistics
    void PrintMemoryStats();
}