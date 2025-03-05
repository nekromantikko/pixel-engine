#pragma once
#include "typedef.h"

enum SpawnerType : TActorSubtype {
	SPAWNER_TYPE_EXP,
	SPAWNER_TYPE_ENEMY,

	SPAWNER_TYPE_COUNT
};

struct ExpSpawnerData {
	TActorPrototypeIndex large;
	TActorPrototypeIndex small;
};

struct ExpSpawnerState {
	u16 remainingValue;
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
