#pragma once
#include "typedef.h"

enum PickupType : TActorSubtype {
	PICKUP_TYPE_EXP,
	PICKUP_TYPE_EXP_REMNANT,

	PICKUP_TYPE_COUNT
};

struct PickupData {
	s16 value;
};

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

#ifdef EDITOR
constexpr const char* PICKUP_TYPE_NAMES[PICKUP_TYPE_COUNT] = { "Exp", "Exp remnant" };
#endif

namespace Game {
	extern const ActorInitFn pickupInitTable[PICKUP_TYPE_COUNT];
	extern const ActorUpdateFn pickupUpdateTable[PICKUP_TYPE_COUNT];
	extern const ActorDrawFn pickupDrawTable[PICKUP_TYPE_COUNT];
}