#include "debug.h"
#include <cstdio>

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