#pragma once
#include "typedef.h"
#include "player_data.h"
#include "enemy_data.h"
#include "bullet_data.h"
#include "pickup_data.h"
#include "effect_data.h"
#include "interactable_data.h"
#include "spawner_data.h"

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
};

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
};

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	constexpr const char* actorTypeNames[ACTOR_TYPE_COUNT] = { "Player", "Enemy", "Bullet", "Pickup", "Effect", "Interactable", "Spawner" };

	inline static const ActorEditorData actorEditorData[ACTOR_TYPE_COUNT] = {
		GetPlayerEditorData(),
		GetEnemyEditorData(),
		GetBulletEditorData(),
		GetPickupEditorData(),
		GetEffectEditorData(),
		GetInteractableEditorData(),
		GetSpawnerEditorData(),
	};
}

#endif