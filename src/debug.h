#pragma once

#define DEBUG_LOG(fmt, ...) Debug::Log("%s: " fmt, \
    __func__, __VA_ARGS__)

#define DEBUG_ERROR(fmt, ...) Debug::Log("[error] %s: " fmt, \
	__func__, __VA_ARGS__)

namespace Debug {
	void Log(const char* fmt, ...);
}