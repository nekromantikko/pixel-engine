#pragma once
#include "typedef.h"

char* AllocFileBytes(const char* fname, u32& outLength);
char* LoadBitmapBytes(const char* fname, u32& outWidth, u32& outHeight, u16& outBpp);