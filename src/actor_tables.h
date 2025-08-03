#pragma once
#include "bullet_data.h"
#include "effect_data.h"
#include "pickup_data.h"
#include "spawner_data.h"
#include "interactable_data.h"
#include "enemy_data.h"

struct Actor;
struct ActorPrototype;

namespace Game {
	// Bullet tables
	extern const ActorInitFn bulletInitTable[BULLET_TYPE_COUNT];
	extern const ActorUpdateFn bulletUpdateTable[BULLET_TYPE_COUNT];
	extern const ActorDrawFn bulletDrawTable[BULLET_TYPE_COUNT];

	// Effect tables
	extern const ActorInitFn effectInitTable[EFFECT_TYPE_COUNT];
	extern const ActorUpdateFn effectUpdateTable[EFFECT_TYPE_COUNT];
	extern const ActorDrawFn effectDrawTable[EFFECT_TYPE_COUNT];

	// Pickup tables
	extern const ActorInitFn pickupInitTable[PICKUP_TYPE_COUNT];
	extern const ActorUpdateFn pickupUpdateTable[PICKUP_TYPE_COUNT];
	extern const ActorDrawFn pickupDrawTable[PICKUP_TYPE_COUNT];

	// Spawner tables
	extern const ActorInitFn spawnerInitTable[SPAWNER_TYPE_COUNT];
	extern const ActorUpdateFn spawnerUpdateTable[SPAWNER_TYPE_COUNT];
	extern const ActorDrawFn spawnerDrawTable[SPAWNER_TYPE_COUNT];

	// Interactable tables
	extern const ActorInitFn interactableInitTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorUpdateFn interactableUpdateTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorDrawFn interactableDrawTable[INTERACTABLE_TYPE_COUNT];

	// Enemy tables
	extern const ActorInitFn enemyInitTable[ENEMY_TYPE_COUNT];
	extern const ActorUpdateFn enemyUpdateTable[ENEMY_TYPE_COUNT];
	extern const ActorDrawFn enemyDrawTable[ENEMY_TYPE_COUNT];

	void EnemyDie(Actor* pActor, const ActorPrototype* pPrototype);
}