#include "memory_arena.h"
#include "debug.h"
#include <cstdlib>
#include <cstring>
#include <cassert>

static constexpr size_t SCRATCH_ARENA_SIZE = 8 * 1024 * 1024; // 8 MB

static Arena g_arenas[ARENA_COUNT];
static bool g_initialized = false;

static void* g_arenaMemory[ARENA_COUNT] = { nullptr };

void ArenaAllocator::Init() {
    if (g_initialized) {
		DEBUG_WARN("Memory arenas already initialized.\n");
		return;
    }

    // Arena sizes
    constexpr size_t arenaSizes[ARENA_COUNT] = {
		SCRATCH_ARENA_SIZE
    };

    // Arena names for debugging
    const char* arenaNames[ARENA_COUNT] = {
        "Scratch",
    };

    for (u32 i = 0; i < ARENA_COUNT; i++) {
		g_arenaMemory[i] = malloc(arenaSizes[i]);
		if (!g_arenaMemory[i]) {
			DEBUG_FATAL("Failed to allocate memory for arena %s (%zu bytes)\n", arenaNames[i], arenaSizes[i]);
			return;
		}
		g_arenas[i].Init(g_arenaMemory[i], arenaSizes[i], arenaNames[i]);
		DEBUG_LOG("Initialized arena %s with size %zu bytes\n", arenaNames[i], arenaSizes[i]);
    }

	g_initialized = true;
	DEBUG_LOG("Memory arenas initialized successfully.\n");
}

void ArenaAllocator::Free() {
	if (!g_initialized) {
		return;
	}

	for (u32 i = 0; i < ARENA_COUNT; i++) {
		if (g_arenaMemory[i]) {
			free(g_arenaMemory[i]);
			g_arenaMemory[i] = nullptr;
		}

		memset(&g_arenas[i], 0, sizeof(Arena)); // Clear arena data
	}

	g_initialized = false;
	DEBUG_LOG("Memory arenas freed successfully.\n");
}

Arena* ArenaAllocator::GetArena(ArenaType type) {
	if (type < 0 || type >= ARENA_COUNT) {
		DEBUG_ERROR("Invalid arena type: %d\n", type);
		return nullptr;
	}

	if (!g_initialized) {
		DEBUG_FATAL("Memory arenas not initialized.\n");
		return nullptr;
	}

	return &g_arenas[type];
}

void* ArenaAllocator::Push(ArenaType type, size_t bytes, size_t alignment) {
	if (type < 0 || type >= ARENA_COUNT) {
		DEBUG_ERROR("Invalid arena type: %d\n", type);
		return nullptr;
	}

	if (!g_initialized) {
		DEBUG_FATAL("Memory arenas not initialized.\n");
		return nullptr;
	}

	Arena* arena = &g_arenas[type];
	void* pResult = arena->Push(bytes, alignment);
	if (!pResult) {
		DEBUG_ERROR("Failed to allocate %zu bytes in arena %s\n", bytes, arena->Name());
		return nullptr;
	}

	DEBUG_LOG("Allocated %zu bytes in arena %s\n", bytes, arena->Name());
	return pResult;
}

void ArenaAllocator::Clear(ArenaType type) {
	if (type < 0 || type >= ARENA_COUNT) {
		DEBUG_ERROR("Invalid arena type: %d\n", type);
		return;
	}

	if (!g_initialized) {
		DEBUG_FATAL("Memory arenas not initialized.\n");
		return;
	}

	Arena* arena = &g_arenas[type];
	arena->Clear();
}