#pragma once
#include "typedef.h"

enum BulletType : TActorSubtype {
	BULLET_TYPE_DEFAULT,
	BULLET_TYPE_GRENADE,

	BULLET_TYPE_COUNT
};

struct BulletData {
	u16 lifetime;
	TActorPrototypeIndex deathEffect;
};

struct BulletState {
	u16 lifetimeCounter;
};

namespace Game {
	extern const ActorInitFn bulletInitTable[BULLET_TYPE_COUNT];
	extern const ActorUpdateFn bulletUpdateTable[BULLET_TYPE_COUNT];
	extern const ActorDrawFn bulletDrawTable[BULLET_TYPE_COUNT];
}

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	extern const ActorEditorData bulletEditorData;
}
#endif