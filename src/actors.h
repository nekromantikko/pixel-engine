#pragma once
#include "typedef.h"
#include "actor_types.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>
#include "memory_pool.h"
#include "asset_types.h"

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

struct ActorPrototype;

struct Actor {
	u64 persistId;

	ActorFlags flags;

	glm::vec2 initialPosition;
	glm::vec2 position;
	glm::vec2 initialVelocity;
	glm::vec2 velocity;

	ActorDrawState drawState;

	ActorState state;

	ActorPrototypeHandle prototypeId;
};

typedef PoolHandle<Actor> ActorHandle;
static constexpr u32 MAX_DYNAMIC_ACTOR_COUNT = 512;
typedef Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> DynamicActorPool;

struct HitResult;

typedef void (*ActorCallbackFn)(Actor*);
typedef void (*ActorCollisionCallbackFn)(Actor*, Actor*);
typedef bool (*ActorFilterFn)(const Actor*);

struct RoomActor;
struct AnimationNew;

namespace Game {
	Actor* SpawnActor(const RoomActor* pTemplate, u32 roomId);
	Actor* SpawnActor(const ActorPrototypeHandle& prototypeId, const glm::vec2& position, const glm::vec2& velocity = {0.0f, 0.0f});
	void ClearActors();

	const ActorPrototypeNew* GetActorPrototype(const Actor* pActor);
	const AnimationNew* GetActorCurrentAnim(const Actor* pActor, const ActorPrototypeNew* pPrototype);
	bool ActorValid(const Actor* pActor);
	bool ActorsColliding(const Actor* pActor, const Actor* pOther);
	void ForEachActorCollision(Actor* pActor, TActorType type, ActorCollisionCallbackFn callback);
	void ForEachActorCollision(Actor* pActor, ActorFilterFn filter, ActorCollisionCallbackFn callback);
	Actor* GetFirstActorCollision(const Actor* pActor, TActorType type);
	Actor* GetFirstActorCollision(const Actor* pActor, ActorFilterFn filter);
	void ForEachActor(TActorType type, ActorCallbackFn callback);
	void ForEachActor(ActorFilterFn filter, ActorCallbackFn callback);
	Actor* GetFirstActor(TActorType type);
	Actor* GetFirstActor(ActorFilterFn filter);

	Actor* GetPlayer();

	bool UpdateCounter(u16& counter);
	void SetDamagePaletteOverride(Actor* pActor, u16 damageCounter);
	void GetAnimFrameFromDirection(Actor* pActor, const ActorPrototypeNew* pPrototype);
	void AdvanceAnimation(u16& animCounter, u16& frameIndex, u16 frameCount, u8 frameLength, s16 loopPoint);
	void AdvanceCurrentAnimation(Actor* pActor, const ActorPrototypeNew* pPrototype);

	void ActorFacePlayer(Actor* pActor);
	bool ActorMoveHorizontal(Actor* pActor, const ActorPrototypeNew* pPrototype, HitResult& outHit);
	bool ActorMoveVertical(Actor* pActor, const ActorPrototypeNew* pPrototype, HitResult& outHit);
	void ApplyGravity(Actor* pActor, r32 gravity = 0.01f);

	Damage CalculateDamage(Actor* pActor, u16 baseDamage);
	u16 ActorTakeDamage(Actor* pActor, const Damage& damage, u16 currentHealth, u16& damageCounter);
	u16 ActorHeal(Actor* pActor, u16 value, u16 currentHealth, u16 maxHealth);

	DynamicActorPool* GetActors(); // TEMP
	void UpdateActors();
	bool DrawActorDefault(const Actor* pActor, const ActorPrototypeNew* pPrototype);
	void DrawActors();
}