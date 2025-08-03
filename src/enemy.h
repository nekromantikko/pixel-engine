#pragma once
#include "enemy_data.h"

struct Actor;
struct ActorPrototype;

namespace Game {
	extern const ActorInitFn enemyInitTable[ENEMY_TYPE_COUNT];
	extern const ActorUpdateFn enemyUpdateTable[ENEMY_TYPE_COUNT];
	extern const ActorDrawFn enemyDrawTable[ENEMY_TYPE_COUNT];

	void EnemyDie(Actor* pActor, const ActorPrototype* pPrototype);
}