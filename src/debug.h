#pragma once
#include <cstdarg>

#define DEBUG_LOG(fmt, ...) Debug::Log("%s: " fmt, \
    __func__ __VA_OPT__(,) __VA_ARGS__)

#define DEBUG_WARN(fmt, ...) Debug::Log("[warn] %s: " fmt, \
	__func__ __VA_OPT__(,) __VA_ARGS__)

#define DEBUG_ERROR(fmt, ...) Debug::Log("[error] %s: " fmt, \
	__func__ __VA_OPT__(,) __VA_ARGS__)

#define DEBUG_FATAL(fmt, ...) do { \
	Debug::Log("[fatal] %s: " fmt, __func__ __VA_OPT__(,) __VA_ARGS__); \
	std::abort(); \
} while (0)

namespace Debug {
#ifdef EDITOR
	void HookEditorDebugLog(void (*callback)(const char* fmt, va_list args));
#endif
	void Log(const char* fmt, ...);
}