#pragma once
#include "typedef.h"

#define DEBUG_LOG(fmt, ...) Print("%s: " fmt, \
    __func__, __VA_ARGS__)

#ifndef DEBUG_ERROR
    #define DEBUG_ERROR(fmt, ...) DEBUG_LOG(fmt, __VA_ARGS__); exit(-1)
#endif

void Print(const char* fmt, ...);
char* AllocFileBytes(const char* fname, u32& outLength);