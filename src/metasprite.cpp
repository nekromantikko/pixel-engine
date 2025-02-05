#include "metasprite.h"
#include "system.h"
#include <stdio.h>

static Metasprite metasprites[MAX_METASPRITE_COUNT];
static char nameMemory[MAX_METASPRITE_COUNT * METASPRITE_MAX_NAME_LENGTH];
static Sprite spriteMemory[MAX_METASPRITE_COUNT * METASPRITE_MAX_SPRITE_COUNT];

Metasprite* Metasprites::GetMetasprite(s32 index) {
	return &metasprites[index];
}

char* Metasprites::GetName(s32 index) {
	return &nameMemory[index * METASPRITE_MAX_NAME_LENGTH];
}

s32 Metasprites::GetIndex(const Metasprite* pMetasprite) {
	s32 index = pMetasprite - metasprites;
	if (index < 0 || index >= MAX_METASPRITE_COUNT) {
		return -1;
	}

	return index;
}

void Metasprites::Clear() {
	for (u32 i = 0; i < MAX_METASPRITE_COUNT; i++) {
		nameMemory[i * METASPRITE_MAX_NAME_LENGTH] = 0;

		metasprites[i].spritesRelativePos = &spriteMemory[i * METASPRITE_MAX_SPRITE_COUNT];
		spriteMemory[i * METASPRITE_MAX_SPRITE_COUNT] = Sprite{};
	}
}

void Metasprites::Load(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "rb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to load metasprite file\n");
	}

	const char signature[4]{};
	fread((void*)signature, sizeof(u8), 4, pFile);

	for (u32 i = 0; i < MAX_METASPRITE_COUNT; i++) {
		fread(&metasprites[i].spriteCount, sizeof(u32), 1, pFile);

		metasprites[i].spritesRelativePos = &spriteMemory[i * METASPRITE_MAX_SPRITE_COUNT];
	}

	fread(nameMemory, METASPRITE_MAX_NAME_LENGTH, MAX_METASPRITE_COUNT, pFile);
	fread(spriteMemory, sizeof(Sprite), METASPRITE_MAX_SPRITE_COUNT * MAX_METASPRITE_COUNT, pFile);

	fclose(pFile);
}

void Metasprites::Save(const char* fname) {
	FILE* pFile;
	fopen_s(&pFile, fname, "wb");

	if (pFile == NULL) {
		DEBUG_ERROR("Failed to write metasprite file\n");
	}

	const char signature[4] = "SPR";
	fwrite(signature, sizeof(u8), 4, pFile);
		
	for (u32 i = 0; i < MAX_METASPRITE_COUNT; i++) {
		fwrite(&metasprites[i].spriteCount, sizeof(u32), 1, pFile);
	}

	fwrite(nameMemory, METASPRITE_MAX_NAME_LENGTH, MAX_METASPRITE_COUNT, pFile);
	fwrite(spriteMemory, sizeof(Sprite), METASPRITE_MAX_SPRITE_COUNT * MAX_METASPRITE_COUNT, pFile);

	fclose(pFile);
}