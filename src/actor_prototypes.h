#pragma once
#include "typedef.h"
#include "collision.h"
#include "actor_types.h"
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

#ifdef EDITOR
#include <imgui_internal.h>

#endif