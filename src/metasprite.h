#pragma once
#include "rendering.h"
#include "collision.h"

constexpr u32 MAX_METASPRITE_COUNT = 256;
constexpr u32 METASPRITE_MAX_NAME_LENGTH = 256;
constexpr u32 METASPRITE_MAX_SPRITE_COUNT = 64;

struct Metasprite {
	u32 spriteCount;
	Sprite* spritesRelativePos;
};

namespace Metasprites {
	Metasprite* GetMetasprite(s32 index);
	Sprite* GetRawSprite(s32 index);
	char* GetName(s32 index);
	char* GetName(const Metasprite* pMetasprite);
	s32 GetIndex(const Metasprite* pMetasprite);
	s32 GetSpriteIndex(const Sprite* pSprite);
	void Copy(s32 srcIndex, s32 dstIndex);

	// Generates empty data
	void Clear();

	void Load(const char* fname);
	void Save(const char* fname);
}