#pragma once
#include "typedef.h"

#define DEBUG_LOG(fmt, ...) Print("%s: " fmt, \
    __func__, __VA_ARGS__)

#ifndef ERROR
    #define ERROR(fmt, ...) DEBUG_LOG(fmt, __VA_ARGS__); exit(-1)
#endif

void Print(const char* fmt, ...);
char* AllocFileBytes(const char* fname, u32& outLength);
char* LoadBitmapBytes(const char* fname, u32& outWidth, u32& outHeight, u16& outBpp);