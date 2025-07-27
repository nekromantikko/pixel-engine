#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "actor_reflection.h"

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
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, large, ASSET_TYPE_ACTOR_PROTOTYPE),
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, small, ASSET_TYPE_ACTOR_PROTOTYPE)
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
	ACTOR_SUBTYPE_PROPERTY_ASSET(LootSpawnerData, types[0], ASSET_TYPE_ACTOR_PROTOTYPE),
	ACTOR_SUBTYPE_PROPERTY_ASSET(LootSpawnerData, types[1], ASSET_TYPE_ACTOR_PROTOTYPE),
	ACTOR_SUBTYPE_PROPERTY_ASSET(LootSpawnerData, types[2], ASSET_TYPE_ACTOR_PROTOTYPE),
	ACTOR_SUBTYPE_PROPERTY_ASSET(LootSpawnerData, types[3], ASSET_TYPE_ACTOR_PROTOTYPE)
);

namespace Game {
	extern const ActorInitFn spawnerInitTable[SPAWNER_TYPE_COUNT];
	extern const ActorUpdateFn spawnerUpdateTable[SPAWNER_TYPE_COUNT];
	extern const ActorDrawFn spawnerDrawTable[SPAWNER_TYPE_COUNT];
}

DECLARE_ACTOR_EDITOR_DATA(spawner)
