#include "memory_arena.h"
#include "debug.h"
#include <cstdlib>
#include <cstring>
#include <cassert>

namespace ArenaAllocator {
    // Arena configuration - adjust these based on game requirements
    static constexpr size_t PERMANENT_ARENA_SIZE = 64 * 1024 * 1024;  // 64MB for assets, game data
    static constexpr size_t TEMPORARY_ARENA_SIZE = 16 * 1024 * 1024;  // 16MB for temporary allocations
    static constexpr size_t VULKAN_ARENA_SIZE = 128 * 1024 * 1024;    // 128MB for Vulkan resources
    
    // Global arena instances
    static MemoryArena g_arenas[ARENA_COUNT];
    static bool g_initialized = false;
    
    // Backing memory for each arena
    static void* g_arenaMemory[ARENA_COUNT] = { nullptr };
    
    void Initialize() {
        if (g_initialized) {
            DEBUG_WARN("ArenaAllocator already initialized!\n");
            return;
        }
        
        // Arena sizes
        constexpr size_t arenaSizes[ARENA_COUNT] = {
            PERMANENT_ARENA_SIZE,
            TEMPORARY_ARENA_SIZE,
            VULKAN_ARENA_SIZE
        };
        
        // Arena names for debugging
        const char* arenaNames[ARENA_COUNT] = {
            "Permanent",
            "Temporary", 
            "Vulkan"
        };
        
        // Allocate backing memory for each arena
        for (u32 i = 0; i < ARENA_COUNT; i++) {
            g_arenaMemory[i] = malloc(arenaSizes[i]);
            if (!g_arenaMemory[i]) {
                DEBUG_FATAL("Failed to allocate memory for %s arena (%zu bytes)\n", 
                           arenaNames[i], arenaSizes[i]);
                // Cleanup already allocated arenas
                for (u32 j = 0; j < i; j++) {
                    free(g_arenaMemory[j]);
                    g_arenaMemory[j] = nullptr;
                }
                return;
            }
            
            // Initialize arena
            g_arenas[i].Initialize(g_arenaMemory[i], arenaSizes[i], arenaNames[i]);
            
            DEBUG_LOG("Initialized %s arena: %zu bytes at %p\n", 
                     arenaNames[i], arenaSizes[i], g_arenaMemory[i]);
        }
        
        g_initialized = true;
        DEBUG_LOG("ArenaAllocator system initialized successfully\n");
        PrintMemoryStats();
    }
    
    void Shutdown() {
        if (!g_initialized) {
            return;
        }
        
        DEBUG_LOG("Shutting down ArenaAllocator system\n");
        PrintMemoryStats();
        
        // Free all backing memory
        for (u32 i = 0; i < ARENA_COUNT; i++) {
            if (g_arenaMemory[i]) {
                free(g_arenaMemory[i]);
                g_arenaMemory[i] = nullptr;
            }
            
            // Clear arena state
            memset(&g_arenas[i], 0, sizeof(MemoryArena));
        }
        
        g_initialized = false;
        DEBUG_LOG("ArenaAllocator system shutdown complete\n");
    }
    
    MemoryArena* GetArena(ArenaType type) {
        assert(g_initialized && "ArenaAllocator not initialized!");
        assert(type >= 0 && type < ARENA_COUNT);
        return &g_arenas[type];
    }
    
    void* Allocate(ArenaType type, size_t bytes, size_t alignment) {
        MemoryArena* arena = GetArena(type);
        return arena->Allocate(bytes, alignment);
    }
    
    ArenaMarker GetMarker(ArenaType type) {
        MemoryArena* arena = GetArena(type);
        return arena->GetMarker();
    }
    
    void ResetToMarker(const ArenaMarker& marker) {
        assert(marker.arena != nullptr);
        marker.arena->ResetToMarker(marker);
    }
    
    void ClearArena(ArenaType type) {
        MemoryArena* arena = GetArena(type);
        arena->Clear();
    }
    
    void ClearTemporaryArena() {
        ClearArena(ARENA_TEMPORARY);
    }
    
    void GetMemoryStats(ArenaType type, size_t& used, size_t& total, r32& percent) {
        MemoryArena* arena = GetArena(type);
        used = arena->used;
        total = arena->size;
        percent = arena->GetUsagePercent();
    }
    
    void PrintMemoryStats() {
        if (!g_initialized) {
            DEBUG_LOG("ArenaAllocator not initialized\n");
            return;
        }
        
        DEBUG_LOG("=== Arena Memory Statistics ===\n");
        size_t totalUsed = 0;
        size_t totalSize = 0;
        
        for (u32 i = 0; i < ARENA_COUNT; i++) {
            const MemoryArena& arena = g_arenas[i];
            totalUsed += arena.used;
            totalSize += arena.size;
            
            DEBUG_LOG("%s Arena: %zu / %zu bytes (%.1f%%) - %zu bytes remaining\n",
                     arena.name,
                     arena.used,
                     arena.size,
                     arena.GetUsagePercent(),
                     arena.GetRemainingBytes());
        }
        
        r32 overallPercent = totalSize > 0 ? (r32(totalUsed) / r32(totalSize)) * 100.0f : 0.0f;
        DEBUG_LOG("Total: %zu / %zu bytes (%.1f%%)\n", totalUsed, totalSize, overallPercent);
        DEBUG_LOG("==============================\n");
    }
}