#pragma once
#include "asset_types.h"
#include "actor_reflection.h"

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

ACTOR_EDITOR_DATA(Enemy,
	{ "slime", GET_SUBTYPE_PROPERTIES(EnemyData) },
	{ "skull", GET_SUBTYPE_PROPERTIES(EnemyData) },
	{ "fireball", GET_SUBTYPE_PROPERTIES(FireballData) }
);