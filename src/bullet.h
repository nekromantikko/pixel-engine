#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "actor_reflection.h"

enum BulletType : TActorSubtype {
	BULLET_TYPE_DEFAULT,
	BULLET_TYPE_GRENADE,

	BULLET_TYPE_COUNT
};

struct BulletData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

ACTOR_SUBTYPE_PROPERTIES(BulletData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(BulletData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(BulletData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)

struct BulletState {
	u16 lifetimeCounter;
};

namespace Game {
	extern const ActorInitFn bulletInitTable[BULLET_TYPE_COUNT];
	extern const ActorUpdateFn bulletUpdateTable[BULLET_TYPE_COUNT];
	extern const ActorDrawFn bulletDrawTable[BULLET_TYPE_COUNT];
}

DECLARE_ACTOR_EDITOR_DATA(bullet)