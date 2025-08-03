#pragma once
#include "typedef.h"
#include "actor_data.h"
#include "actor_behaviour.h"
#include "actor_types.h"
#include "asset_types.h"

struct ActorPrototype;
struct HitResult;
struct RoomActor;
struct Animation;

namespace Game {
	Actor* SpawnActor(const RoomActor* pTemplate, u32 roomId);
	Actor* SpawnActor(const ActorPrototypeHandle& prototypeHandle, const glm::vec2& position, const glm::vec2& velocity = {0.0f, 0.0f});
	void ClearActors();

	const ActorPrototype* GetActorPrototype(const Actor* pActor);
	const Animation* GetActorCurrentAnim(const Actor* pActor, const ActorPrototype* pPrototype);
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
	void GetAnimFrameFromDirection(Actor* pActor, const ActorPrototype* pPrototype);
	void AdvanceAnimation(u16& animCounter, u16& frameIndex, u16 frameCount, u8 frameLength, s16 loopPoint);
	void AdvanceCurrentAnimation(Actor* pActor, const ActorPrototype* pPrototype);

	void ActorFacePlayer(Actor* pActor);
	bool ActorMoveHorizontal(Actor* pActor, const ActorPrototype* pPrototype, HitResult& outHit);
	bool ActorMoveVertical(Actor* pActor, const ActorPrototype* pPrototype, HitResult& outHit);
	void ApplyGravity(Actor* pActor, r32 gravity = 0.01f);

	Damage CalculateDamage(Actor* pActor, u16 baseDamage);
	u16 ActorTakeDamage(Actor* pActor, const Damage& damage, u16 currentHealth, u16& damageCounter);
	u16 ActorHeal(Actor* pActor, u16 value, u16 currentHealth, u16 maxHealth);

	DynamicActorPool* GetActors(); // TEMP
	void UpdateActors();
	bool DrawActorDefault(const Actor* pActor, const ActorPrototype* pPrototype);
	void DrawActors();
}