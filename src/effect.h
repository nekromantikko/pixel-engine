#pragma once
#include "typedef.h"
#include "asset_types.h"

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

struct EffectState {
	u16 initialLifetime;
	u16 lifetimeCounter;
};

struct DamageFlags {
	bool healing : 1;
	bool crit : 1;
	u8 type : 3; // TODO: Add damage types?
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

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	extern const ActorEditorData effectEditorData;
}
#endif