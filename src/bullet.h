#pragma once
#include "typedef.h"

enum BulletType : TActorSubtype {
	BULLET_TYPE_DEFAULT,
	BULLET_TYPE_GRENADE,

	BULLET_TYPE_COUNT
};

struct BulletData {
	u16 lifetime;
	u16 spawnOnDeath;
};

struct BulletState {
	u16 lifetime;
	u16 lifetimeCounter;
};

#ifdef EDITOR
constexpr const char* BULLET_TYPE_NAMES[BULLET_TYPE_COUNT] = { "Default", "Grenade" };
#endif

namespace Game {
	extern const ActorInitFn bulletInitTable[BULLET_TYPE_COUNT];
	extern const ActorUpdateFn bulletUpdateTable[BULLET_TYPE_COUNT];
	extern const ActorDrawFn bulletDrawTable[BULLET_TYPE_COUNT];
}