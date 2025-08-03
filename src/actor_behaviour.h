#pragma once
#include "typedef.h"
#include "player.h"
#include "actor_tables.h"

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