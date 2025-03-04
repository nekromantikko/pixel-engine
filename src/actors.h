#pragma once
#include "typedef.h"
#include "actor_types.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "memory_pool.h"

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

	glm::i16vec2 pixelOffset = {0, 0};

	u8 layer : 2 = 0;
	u8 palette : 2 = 0;
	bool hFlip : 1 = false;
	bool vFlip : 1 = false;
	bool visible : 1 = true;
	bool useCustomPalette : 1 = false;
};

struct Actor;

typedef void (*ActorUpdateFn)(Actor* pActor);
typedef void (*ActorDrawFn)(Actor* pActor);

struct ActorPrototype;

struct Actor {
	u64 id;

	ActorFlags flags;

	glm::vec2 initialPosition;
	glm::vec2 position;
	glm::vec2 initialVelocity;
	glm::vec2 velocity;

	ActorDrawState drawState;

	ActorState state;

	const ActorPrototype* pPrototype;

	ActorUpdateFn pUpdateFn;
	ActorDrawFn pDrawFn;
};

typedef PoolHandle<Actor> ActorHandle;
static constexpr u32 MAX_DYNAMIC_ACTOR_COUNT = 512;
typedef Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> DynamicActorPool;

namespace Game {
	Actor* SpawnActor(const Actor* pTemplate);
	Actor* SpawnActor(const s32 prototypeIndex, const glm::vec2& position, const glm::vec2& velocity = {0.0f, 0.0f});
	void ClearActors();

	bool ActorValid(const Actor* pActor);
	bool ActorsColliding(const Actor* pActor, const Actor* pOther);
	void ForEachActorCollision(Actor* pActor, void (*callback)(Actor*, Actor*), bool (*filter)(const Actor*) = nullptr);
	Actor* GetFirstActorCollision(Actor* pActor, bool (*filter)(const Actor*) = nullptr);
	void ForEachActor(void (*callback)(Actor* pActor), bool (*filter)(const Actor*) = nullptr);
	Actor* GetFirstActor(bool (*filter)(const Actor*) = nullptr);

	Actor* GetPlayer();

	DynamicActorPool* GetActors(); // TEMP
	void UpdateActors(void (*tempCallback)(Actor* pActor));
	bool DrawActorDefault(const Actor* pActor);
	void DrawActors();
}