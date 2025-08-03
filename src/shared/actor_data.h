#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "actor_reflection.h"
#include "player_data.h"

// =================
// ENEMY DATA TYPES
// =================
enum EnemyType : TActorSubtype {
	ENEMY_TYPE_SLIME,
	ENEMY_TYPE_SKULL,
	ENEMY_TYPE_FIREBALL,

	ENEMY_TYPE_COUNT
};

struct EnemyData {
	u16 health;

	u16 expValue;
	ActorPrototypeHandle lootSpawner;
	ActorPrototypeHandle deathEffect;
	ActorPrototypeHandle projectile;
	ActorPrototypeHandle expSpawner;
};

ACTOR_SUBTYPE_PROPERTIES(EnemyData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EnemyData, health, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EnemyData, expValue, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, lootSpawner, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, projectile, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, expSpawner, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)

struct FireballData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

ACTOR_SUBTYPE_PROPERTIES(FireballData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(FireballData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(FireballData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)

struct EnemyState {
	u16 health;
	u16 damageCounter;
};

struct FireballState {
	u16 lifetimeCounter;
};

// =================
// BULLET DATA TYPES
// =================
enum BulletType : TActorSubtype {
	BULLET_TYPE_DEFAULT,
	BULLET_TYPE_GRENADE,

	BULLET_TYPE_COUNT
};

struct BulletData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

ACTOR_SUBTYPE_PROPERTIES(BulletData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(BulletData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(BulletData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)

struct BulletState {
	u16 lifetimeCounter;
};

// =================
// PICKUP DATA TYPES
// =================
enum PickupType : TActorSubtype {
	PICKUP_TYPE_EXP,
	PICKUP_TYPE_EXP_REMNANT,
	PICKUP_TYPE_HEAL,

	PICKUP_TYPE_COUNT
};

struct PickupData {
	s16 value;
	SoundHandle pickupSound;
};

ACTOR_SUBTYPE_PROPERTIES(PickupData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(PickupData, value, S16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PickupData, pickupSound, ASSET_TYPE_SOUND, 1)
)

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

// =================
// EFFECT DATA TYPES
// =================
enum EffectType : TActorSubtype {
	EFFECT_TYPE_NUMBERS,
	EFFECT_TYPE_EXPLOSION,
	EFFECT_TYPE_FEATHER,

	EFFECT_TYPE_COUNT
};

struct EffectData {
	u16 lifetime;
	SoundHandle sound;
};

ACTOR_SUBTYPE_PROPERTIES(EffectData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EffectData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EffectData, sound, ASSET_TYPE_SOUND, 1)
);

struct EffectState {
	u16 initialLifetime;
	u16 lifetimeCounter;
};

struct DamageFlags {
	bool healing : 1;
	bool crit : 1;
	u8 type : 3; // TODO: Add damage types?
};

struct Damage {
	u16 value;
	DamageFlags flags;
};

// Can be treated as EffectState
struct DamageNumberState {
	EffectState base;
	Damage damage;
};

// =======================
// INTERACTABLE DATA TYPES
// =======================
enum CheckpointSubtype : TActorSubtype {
	INTERACTABLE_TYPE_CHECKPOINT,
	INTERACTABLE_TYPE_NPC,

	INTERACTABLE_TYPE_COUNT
};

struct CheckpointData {

};

ACTOR_SUBTYPE_PROPERTIES(CheckpointData)

struct CheckpointState {
	bool activated;
};

struct NPCData {

};

ACTOR_SUBTYPE_PROPERTIES(NPCData)

// =================
// SPAWNER DATA TYPES
// =================
enum SpawnerType : TActorSubtype {
	SPAWNER_TYPE_EXP,
	SPAWNER_TYPE_ENEMY,
	SPAWNER_TYPE_LOOT,

	SPAWNER_TYPE_COUNT
};

struct ExpSpawnerData {
	ActorPrototypeHandle large;
	ActorPrototypeHandle small;
};

ACTOR_SUBTYPE_PROPERTIES(ExpSpawnerData,
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, large, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, small, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
);

struct ExpSpawnerState {
	u16 remainingValue;
};

struct EnemySpawnerData {
};

ACTOR_SUBTYPE_PROPERTIES(EnemySpawnerData);

struct LootSpawnerData {
	u8 typeCount;
	u8 spawnRates[4];
	ActorPrototypeHandle types[4];
};

ACTOR_SUBTYPE_PROPERTIES(LootSpawnerData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(LootSpawnerData, typeCount, U8, 1),
	ACTOR_SUBTYPE_PROPERTY_SCALAR(LootSpawnerData, spawnRates, U8, 4),
	ACTOR_SUBTYPE_PROPERTY_ASSET(LootSpawnerData, types, ASSET_TYPE_ACTOR_PROTOTYPE, 4)
);

// =================
// ACTOR TYPE ENUM AND UNIONS
// =================
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

// =================
// EDITOR DATA (only in editor builds)
// =================
#ifdef EDITOR
#include "editor_actor.h"

// Editor data macros for each type
ACTOR_EDITOR_DATA(Bullet,
	{ "bullet", GET_SUBTYPE_PROPERTIES(BulletData) },
	{ "grenade", GET_SUBTYPE_PROPERTIES(BulletData) }
)

ACTOR_EDITOR_DATA(Effect,
	{ "damage_numbers", GET_SUBTYPE_PROPERTIES(EffectData) },
	{ "explosion", GET_SUBTYPE_PROPERTIES(EffectData) },
	{ "feather", GET_SUBTYPE_PROPERTIES(EffectData) }
)

ACTOR_EDITOR_DATA(Pickup,
	{ "exp", GET_SUBTYPE_PROPERTIES(PickupData) },
	{ "exp_remnant", GET_SUBTYPE_PROPERTIES(PickupData) },
	{ "healing", GET_SUBTYPE_PROPERTIES(PickupData) }
)

ACTOR_EDITOR_DATA(Spawner,
	{ "exp_spawner", GET_SUBTYPE_PROPERTIES(ExpSpawnerData) },
	{ "enemy_spawner", GET_SUBTYPE_PROPERTIES(EnemySpawnerData) },
	{ "loot_spawner", GET_SUBTYPE_PROPERTIES(LootSpawnerData) }
)

ACTOR_EDITOR_DATA(Interactable,
	{ "checkpoint", GET_SUBTYPE_PROPERTIES(CheckpointData) },
	{ "npc", GET_SUBTYPE_PROPERTIES(NPCData) }
)

ACTOR_EDITOR_DATA(Enemy,
	{ "slime", GET_SUBTYPE_PROPERTIES(EnemyData) },
	{ "skull", GET_SUBTYPE_PROPERTIES(EnemyData) },
	{ "fireball", GET_SUBTYPE_PROPERTIES(FireballData) }
)

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