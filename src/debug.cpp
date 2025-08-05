#include "debug.h"
#include "memory_arena.h"
#include <cstdio>
#include <cstring>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

static void Print(const char* fmt, va_list args) {
#ifdef PLATFORM_WINDOWS
	char s[1024];
	vsprintf_s(s, fmt, args);
	OutputDebugString(s);
#else
	vprintf(fmt, args);
#endif
}

#ifdef EDITOR
static void (*editorDebugLogCallback)(const char* fmt, va_list) = nullptr;

void Debug::HookEditorDebugLog(void (*callback)(const char* fmt, va_list args)) {
	editorDebugLogCallback = callback;
}
#endif

void Debug::Log(const char* fmt, ...) {
#ifdef NDEBUG
	return;
#else
	va_list args;
	va_start(args, fmt);
	Print(fmt, args);
#ifdef EDITOR
	if (editorDebugLogCallback) {
		editorDebugLogCallback(fmt, args);
	}
#endif
	va_end(args);
#endif
}

void Debug::TestArenaAllocator() {
	DEBUG_LOG("=== Arena Allocator Test Started ===\n");
	
	// Test basic allocation
	int* testInt = ArenaAllocator::Allocate<int>(ArenaAllocator::ARENA_TEMPORARY);
	*testInt = 12345;
	
	// Test array allocation
	float* testArray = ArenaAllocator::AllocateArray<float>(ArenaAllocator::ARENA_PERMANENT, 10);
	for (int i = 0; i < 10; i++) {
		testArray[i] = (float)i * 3.14f;
	}
	
	// Test marker system
	ArenaMarker marker = ArenaAllocator::GetMarker(ArenaAllocator::ARENA_TEMPORARY);
	char* tempString = ArenaAllocator::AllocateArray<char>(ArenaAllocator::ARENA_TEMPORARY, 64);
	strcpy(tempString, "Arena allocator test string");
	
	DEBUG_LOG("Test int value: %d\n", *testInt);
	DEBUG_LOG("Test array[5]: %.2f\n", testArray[5]);
	DEBUG_LOG("Test string: %s\n", tempString);
	
	ArenaAllocator::PrintMemoryStats();
	
	// Reset to marker (this should free the temp string)
	ArenaAllocator::ResetToMarker(marker);
	
	DEBUG_LOG("After marker reset:\n");
	ArenaAllocator::PrintMemoryStats();
	
	DEBUG_LOG("=== Arena Allocator Test Complete ===\n");
}