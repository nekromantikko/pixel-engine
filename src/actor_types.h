#pragma once
#include "typedef.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "pickup.h"
#include "effect.h"
#include "interactable.h"

enum ActorType : TActorType {
	ACTOR_TYPE_PLAYER,
	ACTOR_TYPE_ENEMY,
	ACTOR_TYPE_BULLET,
	ACTOR_TYPE_PICKUP,
	ACTOR_TYPE_EFFECT,
	ACTOR_TYPE_INTERACTABLE,

	ACTOR_TYPE_COUNT
};

union ActorPrototypeData {
	EnemyData enemyData;
	FireballData fireballData;
	BulletData bulletData;
	PickupData pickupData;
	EffectData effectData;
	CheckpointData checkpointData;
};

union ActorState {
	PlayerState playerState;
	EnemyState enemyState;
	FireballState fireballState;
	BulletState bulletState;
	PickupState pickupState;
	EffectState effectState;
	DamageNumberState dmgNumberState;
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

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	constexpr const char* actorTypeNames[ACTOR_TYPE_COUNT] = { "Player", "Enemy", "Bullet", "Pickup", "Effect", "Interactable" };

	const ActorEditorData actorEditorData[ACTOR_TYPE_COUNT] = {
		playerEditorData,
		enemyEditorData,
		bulletEditorData,
		pickupEditorData,
		effectEditorData,
		interactableEditorData,
	};
}

#endif