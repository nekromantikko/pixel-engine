#pragma once
#include "typedef.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "pickup.h"
#include "effect.h"
#include "interactable.h"
#include "spawner.h"

enum ActorType : TActorType {
	ACTOR_TYPE_PLAYER,
	ACTOR_TYPE_ENEMY,
	ACTOR_TYPE_BULLET,
	ACTOR_TYPE_PICKUP,
	ACTOR_TYPE_EFFECT,
	ACTOR_TYPE_INTERACTABLE,
	ACTOR_TYPE_SPAWNER,

	ACTOR_TYPE_COUNT
};

constexpr u32 ACTOR_PROTOTYPE_DATA_SIZE = 256;

union ActorPrototypeData {
	PlayerData playerData;
	EnemyData enemyData;
	FireballData fireballData;
	BulletData bulletData;
	PickupData pickupData;
	EffectData effectData;
	CheckpointData checkpointData;
	ExpSpawnerData expSpawner;
	LootSpawnerData lootSpawner;

	u8 raw[ACTOR_PROTOTYPE_DATA_SIZE];
};

constexpr u32 ACTOR_STATE_SIZE = 32;

union ActorState {
	PlayerState playerState;
	PlayerOverworldState playerOverworld;
	EnemyState enemyState;
	FireballState fireballState;
	BulletState bulletState;
	PickupState pickupState;
	EffectState effectState;
	DamageNumberState dmgNumberState;
	CheckpointState checkpointState;
	ExpSpawnerState expSpawner;

	u8 raw[ACTOR_STATE_SIZE];
};

namespace Game {
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

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	constexpr const char* actorTypeNames[ACTOR_TYPE_COUNT] = { "Player", "Enemy", "Bullet", "Pickup", "Effect", "Interactable", "Spawner" };

	const ActorEditorData actorEditorData[ACTOR_TYPE_COUNT] = {
		playerEditorData,
		enemyEditorData,
		bulletEditorData,
		pickupEditorData,
		effectEditorData,
		interactableEditorData,
		spawnerEditorData,
	};
}

#include <nlohmann/json.hpp>

NLOHMANN_JSON_SERIALIZE_ENUM(ActorType, {
	{ ACTOR_TYPE_PLAYER, "player" },
	{ ACTOR_TYPE_ENEMY, "enemy" },
	{ ACTOR_TYPE_BULLET, "bullet" },
	{ ACTOR_TYPE_PICKUP, "pickup" },
	{ ACTOR_TYPE_EFFECT, "effect" },
	{ ACTOR_TYPE_INTERACTABLE, "interactable" },
	{ ACTOR_TYPE_SPAWNER, "spawner" }
})

#endif