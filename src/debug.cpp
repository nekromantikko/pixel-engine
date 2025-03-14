#include "debug.h"
#include <cstdarg>
#include <cstdio>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

#ifdef EDITOR
#include "editor.h"
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

void Debug::Log(const char* fmt, ...) {
#ifdef NDEBUG
	return void;
#else
	va_list args;
	va_start(args, fmt);
	Print(fmt, args);
#ifdef EDITOR
	Editor::ConsoleLog(fmt, args);
#endif
	va_end(args);
#endif
}