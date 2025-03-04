#pragma once
#include "typedef.h"

enum PlayerBulletSubtype : u16 {
	BULLET_SUBTYPE_DEFAULT,
	BULLET_SUBTYPE_GRENADE,
	BULLET_SUBTYPE_FIREBALL,

	BULLET_SUBTYPE_COUNT
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
constexpr const char* BULLET_SUBTYPE_NAMES[BULLET_SUBTYPE_COUNT] = { "Default", "Grenade", "Fireball" };
#endif

struct Actor;
struct PersistedActorData;

namespace Game {
	void InitializeBullet(Actor* pActor, const PersistedActorData& persistData);
}