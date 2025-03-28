#pragma once
#include "typedef.h"
#include "collision.h"
#include "actor_types.h"
#include "asset_types.h"

static constexpr u32 MAX_ACTOR_PROTOTYPE_COUNT = 256;
static constexpr u32 ACTOR_PROTOTYPE_MAX_NAME_LENGTH = 256;
static constexpr u32 ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT = 64;

enum AnimationType : u8 {
	ANIMATION_TYPE_SPRITES = 0,
	ANIMATION_TYPE_METASPRITES,

	ANIMATION_TYPE_COUNT
};

struct Animation {
	u8 type;
	u8 frameLength;
	u16 frameCount;
	s16 loopPoint;
	s16 metaspriteIndex;
};

#ifdef EDITOR
constexpr const char* ANIMATION_TYPE_NAMES[ANIMATION_TYPE_COUNT] = { "Sprites", "Metasprites" };
#endif

struct ActorPrototype {
	TActorType type;
	TActorSubtype subtype;
	u8 unused;

	AABB hitbox;

	u32 animCount;
	Animation animations[ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT];

	ActorPrototypeData data;
};

struct ActorPrototypeNew {
	TActorType type;
	TActorSubtype subtype;

	AABB hitbox;

	u32 animCount;
	AnimationHandle animations[ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT];

	ActorPrototypeData data;
};

namespace Assets {
	ActorPrototype* GetActorPrototype(s32 index);
	s32 GetActorPrototypeIndex(const ActorPrototype* pPrototype);
	char* GetActorPrototypeName(s32 index);
	char* GetActorPrototypeName(const ActorPrototype* pPrototype);
	void GetActorPrototypeNames(const char** pOutNames);

	void ClearActorPrototypes();
	void LoadActorPrototypes(const char* fname);
	void SaveActorPrototypes(const char* fname);
}