#pragma once
#include "typedef.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "pickup.h"
#include "effect.h"
#include "interactable.h"

#pragma region Common
enum ActorType : TActorType {
	ACTOR_TYPE_PLAYER,
	ACTOR_TYPE_ENEMY,
	ACTOR_TYPE_BULLET,
	ACTOR_TYPE_PICKUP,
	ACTOR_TYPE_EFFECT,
	ACTOR_TYPE_INTERACTABLE,

	ACTOR_TYPE_COUNT
};

#ifdef EDITOR
constexpr const char* ACTOR_TYPE_NAMES[ACTOR_TYPE_COUNT] = { "Player", "Enemy", "Bullet", "Pickup", "Effect", "Interactable" };
#endif
#pragma endregion

union ActorPrototypeData {
	EnemyData npcData;
	BulletData bulletData;
	PickupData pickupData;
	EffectData effectData;
	CheckpointData checkpointData;
};

union ActorState {
	PlayerState playerState;
	EnemyState npcState;
	BulletState bulletState;
	PickupState pickupState;
	EffectState effectState;
	CheckpointState checkpointState;
};

namespace Game {
	constexpr ActorInitFn const* actorInitTable[ACTOR_TYPE_COUNT] = {
		playerInitTable,
		enemyInitTable,
		bulletInitTable,
		pickupInitTable,
		effectInitTable,
		interactableInitTable,
	};

	constexpr ActorUpdateFn const* actorUpdateTable[ACTOR_TYPE_COUNT] = {
		playerUpdateTable,
		enemyUpdateTable,
		bulletUpdateTable,
		pickupUpdateTable,
		effectUpdateTable,
		interactableUpdateTable,
	};

	constexpr ActorDrawFn const* actorDrawTable[ACTOR_TYPE_COUNT] = {
		playerDrawTable,
		enemyDrawTable,
		bulletDrawTable,
		pickupDrawTable,
		effectDrawTable,
		interactableDrawTable,
	};
}