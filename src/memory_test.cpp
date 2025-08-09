#include <SDL.h>
#include <cfloat>
#include "memory_arena.h"
#include "memory_profiler.h"
#include "input.h"
#include <cstdio>
#include <cstdlib>

static constexpr const char* WINDOW_TITLE = "Nekro Pixel Engine - Memory Test";

int main(int argc, char** argv) {
    printf("Starting memory profiling test...\n");
    MemoryProfiler::Init();
    
    ArenaAllocator::Init();
    MemoryProfiler::LogMemoryPoint("AFTER_ARENA_INIT");

    // Skip asset loading for memory test
    MemoryProfiler::LogMemoryPoint("SKIPPING_ASSET_LOADING");

    printf("Initializing SDL...\n");
    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_HAPTIC);
    MemoryProfiler::LogMemoryPoint("AFTER_SDL_INIT");
    
    printf("Creating window...\n");
    SDL_Window* pWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1536, 864, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    if (!pWindow) {
        printf("Failed to create window: %s\n", SDL_GetError());
        MemoryProfiler::LogMemoryPoint("WINDOW_CREATION_FAILED");
    } else {
        MemoryProfiler::LogMemoryPoint("AFTER_WINDOW_CREATION");
    }

    printf("Sleeping for 2 seconds to measure stable memory...\n");
    SDL_Delay(2000);
    MemoryProfiler::LogMemoryPoint("AFTER_2_SECOND_DELAY");
    
    // Let's also test allocating some memory in the arenas
    printf("Testing arena allocations...\n");
    void* test_alloc1 = ArenaAllocator::Push(ARENA_PERMANENT, 1024 * 1024); // 1MB
    MemoryProfiler::LogMemoryPoint("AFTER_1MB_ARENA_ALLOC");
    
    void* test_alloc2 = ArenaAllocator::Push(ARENA_SCRATCH, 2 * 1024 * 1024); // 2MB
    MemoryProfiler::LogMemoryPoint("AFTER_2MB_ARENA_ALLOC");
    
    // Test some regular heap allocations to compare
    printf("Testing heap allocations...\n");
    void* heap_alloc1 = malloc(10 * 1024 * 1024); // 10MB
    MemoryProfiler::LogMemoryPoint("AFTER_10MB_HEAP_ALLOC");
    
    void* heap_alloc2 = malloc(50 * 1024 * 1024); // 50MB
    MemoryProfiler::LogMemoryPoint("AFTER_50MB_HEAP_ALLOC");
    
    printf("Sleeping 2 more seconds...\n");
    SDL_Delay(2000);
    MemoryProfiler::LogMemoryPoint("FINAL_MEASUREMENT");
    
    // Cleanup - report final memory before freeing arenas
    MemoryProfiler::ReportFinal();
    
    printf("Cleaning up...\n");
    free(heap_alloc2);
    free(heap_alloc1);
    
    if (pWindow) {
        SDL_DestroyWindow(pWindow);
    }
    SDL_Quit();

    ArenaAllocator::Free();
    printf("Memory test completed.\n");
    return 0;
}