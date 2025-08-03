#pragma once
#include "typedef.h"
#include "collision_types.h"
#include "actor_data.h"
#include "asset_types.h"

struct ActorPrototype {
	TActorType type;
	TActorSubtype subtype;

	AABB hitbox;

	ActorPrototypeData data;

	u32 animCount;
	u32 animOffset;

	inline AnimationHandle* GetAnimations() const {
		return (AnimationHandle*)((u8*)this + animOffset);
	}
};