#pragma once
#include "typedef.h"

struct MemoryUsage {
    size_t rss_kb;      // Resident Set Size in KB
    size_t vsize_kb;    // Virtual memory size in KB
    size_t peak_rss_kb; // Peak RSS usage in KB
};

namespace MemoryProfiler {
    // Get current memory usage
    MemoryUsage GetCurrentUsage();
    
    // Get memory usage from arena allocator
    void GetArenaUsage(size_t& total_allocated, size_t& total_used);
    
    // Mark a profiling point with a name and log memory usage
    void LogMemoryPoint(const char* point_name);
    
    // Initialize profiling (call at start of main)
    void Init();
    
    // Report final memory statistics (call at end of main)
    void ReportFinal();
}