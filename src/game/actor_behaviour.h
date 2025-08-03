#pragma once
#include "typedef.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "pickup.h"
#include "effect.h"
#include "interactable.h"
#include "spawner.h"

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