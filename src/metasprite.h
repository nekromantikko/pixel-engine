#pragma once
#include "rendering.h"
#include "collision.h"

namespace Metasprite {
	
	constexpr u32 maxMetaspriteCount = 256;
	constexpr u32 metaspriteMaxNameLength = 256;
	constexpr u32 metaspriteMaxSpriteCount = 64;
	constexpr u32 metaspriteMaxColliderCount = 8;

	struct Metasprite {
		char *name;
		u32 spriteCount;
		Sprite* spritesRelativePos;
		u32 colliderCount;
		Collision::Collider* colliders;
	};

	Metasprite* GetMetaspritesPtr();

	// Generates empty data
	void InitializeMetasprites();

	void LoadMetasprites(const char* fname);
	void SaveMetasprites(const char* fname);
}