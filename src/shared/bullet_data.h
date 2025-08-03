#pragma once
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

ACTOR_EDITOR_DATA(Bullet,
	{ "bullet", GET_SUBTYPE_PROPERTIES(BulletData) },
	{ "grenade", GET_SUBTYPE_PROPERTIES(BulletData) }
);