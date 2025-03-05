#pragma once
#include "typedef.h"

enum EffectType : TActorSubtype {
	EFFECT_TYPE_NUMBERS,
	EFFECT_TYPE_EXPLOSION,
	EFFECT_TYPE_FEATHER,

	EFFECT_TYPE_COUNT
};

struct EffectData {
	u16 lifetime;
};

struct EffectState {
	u16 initialLifetime;
	u16 lifetimeCounter;
};

struct DamageNumberState {
	s16 value;
	u16 lifetimeCounter;
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