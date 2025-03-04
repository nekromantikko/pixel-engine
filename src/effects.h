#pragma once
#include "typedef.h"

enum EffectSubtype : u16 {
	EFFECT_SUBTYPE_NUMBERS,
	EFFECT_SUBTYPE_EXPLOSION,
	EFFECT_SUBTYPE_FEATHER,

	EFFECT_SUBTYPE_COUNT
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
constexpr const char* EFFECT_SUBTYPE_NAMES[EFFECT_SUBTYPE_COUNT] = { "Numbers", "Explosion", "Feather" };
#endif

struct Actor;
struct PersistedActorData;

namespace Game {
	void InitializeEffect(Actor* pActor, const PersistedActorData& persistData);
}