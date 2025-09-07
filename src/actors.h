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
	TActorType type;
	TActorSubtype subtype;
	u64 persistId;

	ActorFlags flags;

	glm::vec2 initialPosition;
	glm::vec2 position;
	glm::vec2 initialVelocity;
	glm::vec2 velocity;

	ActorDrawState drawState;

	ActorData data;
	AABB hitbox;

	// Temporary until animation system refactor
	ActorPrototypeHandle __temp_actorPrototypeHandle;
};

struct PersistedActorData;
struct RoomActor;
struct HitResult;

typedef PoolHandle<Actor> ActorHandle;
static constexpr u32 MAX_DYNAMIC_ACTOR_COUNT = 512;
typedef Pool<Actor, MAX_DYNAMIC_ACTOR_COUNT> DynamicActorPool;

typedef void (*ActorCallbackFn)(Actor*);
typedef void (*ActorCollisionCallbackFn)(Actor*, Actor*);
typedef bool (*ActorFilterFn)(const Actor*);

typedef void (*ActorInitFn)(Actor*, const PersistedActorData*);
typedef void (*ActorUpdateFn)(Actor*);
typedef bool (*ActorDrawFn)(const Actor*);

enum PlayerWeaponType : u8 {
	PLAYER_WEAPON_BOW,
	PLAYER_WEAPON_LAUNCHER
};

enum PlayerModeBits : u8 {
	PLAYER_MODE_NORMAL,
	PLAYER_MODE_STAND_TO_SIT,
	PLAYER_MODE_SITTING,
	PLAYER_MODE_SIT_TO_STAND,
	PLAYER_MODE_DYING,
	PLAYER_MODE_DAMAGED,
	PLAYER_MODE_ENTERING,
	PLAYER_MODE_DODGE,
};

namespace Game {
	Actor* SpawnActor(const RoomActor* pTemplate, u32 roomId);
	Actor* SpawnActor(const ActorPrototypeHandle& prototypeHandle, const glm::vec2& position, const glm::vec2& velocity = {0.0f, 0.0f});
	void ClearActors();

	const Animation* GetActorCurrentAnim(const Actor* pActor);
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
	bool PlayerInvulnerable(Actor* pPlayer);
	void PlayerTakeDamage(Actor* pPlayer, const Damage& damage, const glm::vec2& enemyPos);
	void PlayerRespawnAtCheckpoint(Actor* pPlayer);

	void EnemyDie(Actor* pActor);

	bool UpdateCounter(u16& counter);
	void SetDamagePaletteOverride(Actor* pActor, u16 damageCounter);
	void GetAnimFrameFromDirection(Actor* pActor);
	void AdvanceAnimation(u16& animCounter, u16& frameIndex, u16 frameCount, u8 frameLength, s16 loopPoint);
	void AdvanceCurrentAnimation(Actor* pActor);

	void ActorFacePlayer(Actor* pActor);
	bool ActorMoveHorizontal(Actor* pActor, HitResult& outHit);
	bool ActorMoveVertical(Actor* pActor, HitResult& outHit);
	void ApplyGravity(Actor* pActor, r32 gravity = 0.01f);

	Damage CalculateDamage(Actor* pActor, u16 baseDamage);
	u16 ActorTakeDamage(Actor* pActor, const Damage& damage, u16 currentHealth, u16& damageCounter);
	u16 ActorHeal(Actor* pActor, u16 value, u16 currentHealth, u16 maxHealth);

	DynamicActorPool* GetActors(); // TEMP
	void UpdateActors();
	bool DrawActorDefault(const Actor* pActor);
	void DrawActors();

	extern const ActorInitFn playerInitTable[PLAYER_TYPE_COUNT];
	extern const ActorUpdateFn playerUpdateTable[PLAYER_TYPE_COUNT];
	extern const ActorDrawFn playerDrawTable[PLAYER_TYPE_COUNT];

	extern const ActorInitFn enemyInitTable[ENEMY_TYPE_COUNT];
	extern const ActorUpdateFn enemyUpdateTable[ENEMY_TYPE_COUNT];
	extern const ActorDrawFn enemyDrawTable[ENEMY_TYPE_COUNT];

	extern const ActorInitFn bulletInitTable[BULLET_TYPE_COUNT];
	extern const ActorUpdateFn bulletUpdateTable[BULLET_TYPE_COUNT];
	extern const ActorDrawFn bulletDrawTable[BULLET_TYPE_COUNT];

	extern const ActorInitFn pickupInitTable[PICKUP_TYPE_COUNT];
	extern const ActorUpdateFn pickupUpdateTable[PICKUP_TYPE_COUNT];
	extern const ActorDrawFn pickupDrawTable[PICKUP_TYPE_COUNT];

	extern const ActorInitFn effectInitTable[EFFECT_TYPE_COUNT];
	extern const ActorUpdateFn effectUpdateTable[EFFECT_TYPE_COUNT];
	extern const ActorDrawFn effectDrawTable[EFFECT_TYPE_COUNT];

	extern const ActorInitFn interactableInitTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorUpdateFn interactableUpdateTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorDrawFn interactableDrawTable[INTERACTABLE_TYPE_COUNT];

	extern const ActorInitFn spawnerInitTable[SPAWNER_TYPE_COUNT];
	extern const ActorUpdateFn spawnerUpdateTable[SPAWNER_TYPE_COUNT];
	extern const ActorDrawFn spawnerDrawTable[SPAWNER_TYPE_COUNT];

	constexpr ActorInitFn const* actorInitTable[ACTOR_TYPE_COUNT] = {
		playerInitTable,
		enemyInitTable,
		bulletInitTable,
		pickupInitTable,
		effectInitTable,
		interactableInitTable,
		spawnerInitTable,
	};

	constexpr ActorUpdateFn const* actorUpdateTable[ACTOR_TYPE_COUNT] = {
		playerUpdateTable,
		enemyUpdateTable,
		bulletUpdateTable,
		pickupUpdateTable,
		effectUpdateTable,
		interactableUpdateTable,
		spawnerUpdateTable,
	};

	constexpr ActorDrawFn const* actorDrawTable[ACTOR_TYPE_COUNT] = {
		playerDrawTable,
		enemyDrawTable,
		bulletDrawTable,
		pickupDrawTable,
		effectDrawTable,
		interactableDrawTable,
		spawnerDrawTable,
	};
}