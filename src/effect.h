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
	u16 lifetime;
	u16 lifetimeCounter;
	s16 value;
};

#ifdef EDITOR
constexpr const char* EFFECT_TYPE_NAMES[EFFECT_TYPE_COUNT] = { "Numbers", "Explosion", "Feather" };
#endif

namespace Game {
	extern const ActorInitFn effectInitTable[EFFECT_TYPE_COUNT];
	extern const ActorUpdateFn effectUpdateTable[EFFECT_TYPE_COUNT];
	extern const ActorDrawFn effectDrawTable[EFFECT_TYPE_COUNT];
}