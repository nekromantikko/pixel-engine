#pragma once
#include "typedef.h"
#include "asset_types.h"
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

struct FireballData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

struct EnemyState {
	u16 health;
	u16 damageCounter;
};

struct FireballState {
	u16 lifetimeCounter;
};

struct Actor;
struct ActorPrototypeNew;

namespace Game {
	extern const ActorInitFn enemyInitTable[ENEMY_TYPE_COUNT];
	extern const ActorUpdateFn enemyUpdateTable[ENEMY_TYPE_COUNT];
	extern const ActorDrawFn enemyDrawTable[ENEMY_TYPE_COUNT];

	void EnemyDie(Actor* pActor, const ActorPrototypeNew* pPrototype);
}

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	extern const ActorEditorData enemyEditorData;
}
#endif