#pragma once
#include "typedef.h"

enum NPCSubtype : TActorSubtype {
	ENEMY_TYPE_SLIME,
	ENEMY_TYPE_SKULL,
	ENEMY_TYPE_FIREBALL,

	ENEMY_TYPE_COUNT
};

struct EnemyData {
	u16 health;

	u16 expValue;
	u16 lootType;
	u16 spawnOnDeath;
};

struct EnemyState {
	u16 health;
	u16 damageCounter;
};

#ifdef EDITOR
constexpr const char* ENEMY_TYPE_NAMES[ENEMY_TYPE_COUNT] = { "Enemy Slime", "Enemy Skull", "Fireball" };
#endif

struct Actor;

namespace Game {
	extern const ActorInitFn enemyInitTable[ENEMY_TYPE_COUNT];
	extern const ActorUpdateFn enemyUpdateTable[ENEMY_TYPE_COUNT];
	extern const ActorDrawFn enemyDrawTable[ENEMY_TYPE_COUNT];

	void EnemyDie(Actor* pActor);
}