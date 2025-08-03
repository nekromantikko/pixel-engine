#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "typedef.h"
#include "memory_pool.h"
#include "actor_data.h"

enum ActorFacingDir : s8 {
	ACTOR_FACING_LEFT = -1, // 0b11
	ACTOR_FACING_RIGHT = 1 // 0b01
};

struct ActorFlags {
	s8 facingDir : 2;
	bool inAir : 1;
	bool active : 1;
	bool pendingRemoval : 1;
};

struct ActorDrawState {
	u16 animIndex = 0;
	u16 frameIndex = 0;
	u16 animCounter = 0;

	glm::i16vec2 pixelOffset = { 0, 0 };

	u8 layer : 2 = 0;
	u8 palette : 2 = 0;
	bool hFlip : 1 = false;
	bool vFlip : 1 = false;
	bool visible : 1 = true;
	bool useCustomPalette : 1 = false;
};

struct Actor {
	u64 persistId;

	ActorFlags flags;

	glm::vec2 initialPosition;
	glm::vec2 position;
	glm::vec2 initialVelocity;
	glm::vec2 velocity;

	ActorDrawState drawState;

	ActorState state;

	ActorPrototypeHandle prototypeHandle;
};

typedef PoolHandle<Actor> ActorHandle;
static constexpr u32 MAX_DYNAMIC_ACTOR_COUNT = 512;
typedef Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> DynamicActorPool;

struct HitResult;

typedef void (*ActorCallbackFn)(Actor*);
typedef void (*ActorCollisionCallbackFn)(Actor*, Actor*);
typedef bool (*ActorFilterFn)(const Actor*);