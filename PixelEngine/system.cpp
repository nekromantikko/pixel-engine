#include "system.h"
#include <cstdio>
#include <stdarg.h>
#include <windows.h>
#include <atlimage.h>
#include <iostream>
#include <fstream>

void Print(const char* fmt, ...) {
	char s[1025];
	va_list args;
	va_start(args, fmt);
	wvsprintf(s, fmt, args);
	va_end(args);
	OutputDebugString(s);
}

// TODO: Do this without stl later...
// Memory must be freed later
char* AllocFileBytes(const char* fname, u32& outLength) {
	std::ifstream file(fname, std::ios::ate | std::ios::binary);
	u32 fileSize = file.tellg();
	char *buffer = (char*)malloc(fileSize);
	file.seekg(0);
	file.read(buffer, fileSize);
	file.close();

	outLength = fileSize;
	return buffer;
}

#pragma pack(push, 1)
struct BitmapHeader {
	u16 fileType;
	u32 fileSize;
	u16 reserved1;
	u16 reserved2;
	u32 bitmapOffset;
	u32 size;
	s32 width;
	s32 height;
	u16 planes;
	u16 bpp;
};
#pragma pack(pop)

char* LoadBitmapBytes(const char* fname, u32& outWidth, u32& outHeight, u16& outBpp) {
	u32 fileSize;
	char* bmpData = AllocFileBytes(fname, fileSize);
	if (fileSize == 0) {
		ERROR("Failed to read file!\n");
	}

	BitmapHeader* header = (BitmapHeader*)bmpData;
	DEBUG_LOG("Type = %d, size = %d, offset = %d, width = %d, height = %d, bpp = %d\n", header->fileType, header->fileSize, header->bitmapOffset, header->width, header->height, header->bpp);
	
	u32 size = header->width * header->height * header->bpp;
	char* pixels = (char*)malloc(size);
	memcpy(pixels, (void*)(bmpData + header->bitmapOffset), size);
	outWidth = header->width;
	outHeight = header->height;
	outBpp = header->bpp;
	free(bmpData);
	return pixels;
}