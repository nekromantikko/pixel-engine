#include "memory_profiler.h"
#include "memory_arena.h"
#include "debug.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>

// Use the same arena size macros as memory_arena.cpp
#ifndef PERMANENT_ARENA_SIZE_MB
#define PERMANENT_ARENA_SIZE_MB 4
#endif

#ifndef ASSET_ARENA_SIZE_MB  
#define ASSET_ARENA_SIZE_MB 2
#endif

#ifndef SCRATCH_ARENA_SIZE_MB
#define SCRATCH_ARENA_SIZE_MB 2
#endif

static MemoryUsage s_initial_usage = {0, 0, 0};
static MemoryUsage s_previous_usage = {0, 0, 0};
static bool s_initialized = false;

MemoryUsage MemoryProfiler::GetCurrentUsage() {
    MemoryUsage usage = {0, 0, 0};
    
    // Read from /proc/self/stat for current process memory info
    std::ifstream stat_file("/proc/self/stat");
    if (!stat_file.is_open()) {
        return usage;
    }
    
    std::string line;
    if (std::getline(stat_file, line)) {
        std::istringstream iss(line);
        std::string token;
        
        // Skip to field 23 (vsize) and 24 (rss) 
        for (int i = 0; i < 22; i++) {
            iss >> token;
        }
        
        // Field 23: vsize (virtual memory size in bytes)
        iss >> token;
        usage.vsize_kb = std::stoull(token) / 1024;
        
        // Field 24: rss (resident set size in pages)
        iss >> token;
        long page_size = sysconf(_SC_PAGESIZE);
        usage.rss_kb = (std::stoull(token) * page_size) / 1024;
    }
    
    // Get peak RSS from /proc/self/status
    std::ifstream status_file("/proc/self/status");
    if (status_file.is_open()) {
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.find("VmHWM:") == 0) {
                // VmHWM is peak RSS in kB
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                usage.peak_rss_kb = std::stoull(value);
                break;
            }
        }
    }
    
    return usage;
}

void MemoryProfiler::GetArenaUsage(size_t& total_allocated, size_t& total_used) {
    total_allocated = 0;
    total_used = 0;
    
    for (int i = 0; i < ARENA_COUNT; i++) {
        Arena* arena = ArenaAllocator::GetArena(static_cast<ArenaType>(i));
        if (arena) {
            // Use the actual arena size constants from memory_arena.cpp
            switch (i) {
                case ARENA_PERMANENT:
                    total_allocated += PERMANENT_ARENA_SIZE_MB * 1024 * 1024;
                    break;
                case ARENA_ASSETS:
                    total_allocated += ASSET_ARENA_SIZE_MB * 1024 * 1024;
                    break;
                case ARENA_SCRATCH:
                    total_allocated += SCRATCH_ARENA_SIZE_MB * 1024 * 1024;
                    break;
            }
            total_used += arena->Size();
        }
    }
}

void MemoryProfiler::LogMemoryPoint(const char* point_name) {
    if (!s_initialized) {
        DEBUG_WARN("MemoryProfiler not initialized\n");
        return;
    }
    
    MemoryUsage current = GetCurrentUsage();
    size_t arena_allocated = 0, arena_used = 0;
    
    // Only get arena usage if arenas are initialized
    if (ArenaAllocator::GetArena(ARENA_PERMANENT) != nullptr) {
        GetArenaUsage(arena_allocated, arena_used);
    }
    
    // Calculate deltas
    long rss_delta = (long)current.rss_kb - (long)s_previous_usage.rss_kb;
    long vsize_delta = (long)current.vsize_kb - (long)s_previous_usage.vsize_kb;
    
    DEBUG_LOG("=== MEMORY PROFILE: %s ===\n", point_name);
    DEBUG_LOG("RSS: %zu KB (delta: %+ld KB) | Peak RSS: %zu KB\n", 
              current.rss_kb, rss_delta, current.peak_rss_kb);
    DEBUG_LOG("VSize: %zu KB (delta: %+ld KB)\n", 
              current.vsize_kb, vsize_delta);
    
    if (arena_allocated > 0) {
        DEBUG_LOG("Arena: %zu KB allocated, %zu KB used (%.1f%% utilization)\n",
                  arena_allocated / 1024, arena_used / 1024,
                  (100.0 * arena_used) / arena_allocated);
        DEBUG_LOG("Non-arena memory: ~%zu KB\n", 
                  current.rss_kb > (arena_used / 1024) ? current.rss_kb - (arena_used / 1024) : 0);
    } else {
        DEBUG_LOG("Arenas not yet initialized\n");
    }
    DEBUG_LOG("==========================================\n");
    
    s_previous_usage = current;
}

void MemoryProfiler::Init() {
    s_initial_usage = GetCurrentUsage();
    s_previous_usage = s_initial_usage;
    s_initialized = true;
    
    printf("Memory profiler initialized\n");
    printf("=== MEMORY PROFILE: STARTUP ===\n");
    printf("RSS: %zu KB | Peak RSS: %zu KB\n", s_initial_usage.rss_kb, s_initial_usage.peak_rss_kb);
    printf("VSize: %zu KB\n", s_initial_usage.vsize_kb);
    printf("Arenas not yet initialized\n");
    printf("==========================================\n");
}

void MemoryProfiler::ReportFinal() {
    if (!s_initialized) {
        return;
    }
    
    MemoryUsage final_usage = GetCurrentUsage();
    size_t arena_allocated = 0, arena_used = 0;
    
    // Only get arena usage if arenas are still valid
    if (ArenaAllocator::GetArena(ARENA_PERMANENT) != nullptr) {
        GetArenaUsage(arena_allocated, arena_used);
    }
    
    DEBUG_LOG("\n=== FINAL MEMORY REPORT ===\n");
    DEBUG_LOG("Initial RSS: %zu KB\n", s_initial_usage.rss_kb);
    DEBUG_LOG("Final RSS: %zu KB\n", final_usage.rss_kb);
    DEBUG_LOG("Peak RSS: %zu KB\n", final_usage.peak_rss_kb);
    DEBUG_LOG("Total growth: %+ld KB\n", (long)final_usage.rss_kb - (long)s_initial_usage.rss_kb);
    
    if (arena_allocated > 0) {
        DEBUG_LOG("Arena memory: %zu KB allocated, %zu KB used\n", 
                  arena_allocated / 1024, arena_used / 1024);
        DEBUG_LOG("Estimated non-arena memory: %zu KB\n",
                  final_usage.rss_kb > (arena_used / 1024) ? final_usage.rss_kb - (arena_used / 1024) : 0);
    }
    DEBUG_LOG("=============================\n");
}