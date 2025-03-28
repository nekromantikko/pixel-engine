#pragma once
#include "typedef.h"
#include "asset_types.h"

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

struct ExpSpawnerState {
	u16 remainingValue;
};

struct LootSpawnerData {
	u8 typeCount;
	u8 spawnRates[4];
	ActorPrototypeHandle types[4];
};

namespace Game {
	extern const ActorInitFn spawnerInitTable[SPAWNER_TYPE_COUNT];
	extern const ActorUpdateFn spawnerUpdateTable[SPAWNER_TYPE_COUNT];
	extern const ActorDrawFn spawnerDrawTable[SPAWNER_TYPE_COUNT];
}

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	extern const ActorEditorData spawnerEditorData;
}
#endif
