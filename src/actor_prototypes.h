#pragma once
#include "typedef.h"
#include "collision.h"
#include "actor_types.h"
#include "asset_types.h"

static constexpr u32 ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT = 64;

struct ActorPrototype {
	TActorType type;
	TActorSubtype subtype;

	AABB hitbox;

	u32 animCount;
	AnimationHandle animations[ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT];

	ActorPrototypeDataNew data;
};