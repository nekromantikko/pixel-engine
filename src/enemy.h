#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "actor_reflection.h"
#include <cstddef>

enum NPCSubtype : TActorSubtype {
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
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, lootSpawner, ASSET_TYPE_ACTOR_PROTOTYPE),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, projectile, ASSET_TYPE_ACTOR_PROTOTYPE),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, expSpawner, ASSET_TYPE_ACTOR_PROTOTYPE)
)

struct FireballData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

ACTOR_SUBTYPE_PROPERTIES(FireballData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(FireballData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(FireballData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE)
)

struct EnemyState {
	u16 health;
	u16 damageCounter;
};

struct FireballState {
	u16 lifetimeCounter;
};

struct Actor;
struct ActorPrototype;

namespace Game {
	extern const ActorInitFn enemyInitTable[ENEMY_TYPE_COUNT];
	extern const ActorUpdateFn enemyUpdateTable[ENEMY_TYPE_COUNT];
	extern const ActorDrawFn enemyDrawTable[ENEMY_TYPE_COUNT];

	void EnemyDie(Actor* pActor, const ActorPrototype* pPrototype);
}

DECLARE_ACTOR_EDITOR_DATA(enemy)