#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "actor_reflection.h"

enum EffectType : TActorSubtype {
	EFFECT_TYPE_NUMBERS,
	EFFECT_TYPE_EXPLOSION,
	EFFECT_TYPE_FEATHER,

	EFFECT_TYPE_COUNT
};

struct EffectData {
	u16 lifetime;
	SoundHandle sound;
};

ACTOR_SUBTYPE_PROPERTIES(EffectData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EffectData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EffectData, sound, ASSET_TYPE_SOUND, 1)
);

struct EffectState {
	u16 initialLifetime;
	u16 lifetimeCounter;
};

enum DamageType : u8 {
	DAMAGE_TYPE_PHYSICAL = 0,
	DAMAGE_TYPE_FIRE = 1,
	DAMAGE_TYPE_ICE = 2,
	DAMAGE_TYPE_LIGHTNING = 3,
	DAMAGE_TYPE_POISON = 4,
	// 3 bits allows for 8 types total (0-7)
	DAMAGE_TYPE_COUNT = 5
};

struct DamageFlags {
	bool healing : 1;
	bool crit : 1;
	u8 type : 3; // DamageType enum
};

struct Damage {
	u16 value;
	DamageFlags flags;
};

// Can be treated as EffectState
struct DamageNumberState {
	EffectState base;
	Damage damage;
};

#ifdef EDITOR
constexpr const char* EFFECT_TYPE_NAMES[EFFECT_TYPE_COUNT] = { "Damage numbers", "Explosion", "Feather" };
#endif

namespace Game {
	extern const ActorInitFn effectInitTable[EFFECT_TYPE_COUNT];
	extern const ActorUpdateFn effectUpdateTable[EFFECT_TYPE_COUNT];
	extern const ActorDrawFn effectDrawTable[EFFECT_TYPE_COUNT];
}

DECLARE_ACTOR_EDITOR_DATA(effect)