#pragma once
#include "typedef.h"
#include "asset_types.h"
#include "actor_reflection.h"

enum PickupType : TActorSubtype {
	PICKUP_TYPE_EXP,
	PICKUP_TYPE_EXP_REMNANT,
	PICKUP_TYPE_HEAL,

	PICKUP_TYPE_COUNT
};

struct PickupData {
	s16 value;
	SoundHandle pickupSound;
};

ACTOR_SUBTYPE_PROPERTIES(PickupData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(PickupData, value, S16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PickupData, pickupSound, ASSET_TYPE_SOUND, 1)
)

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

namespace Game {
	extern const ActorInitFn pickupInitTable[PICKUP_TYPE_COUNT];
	extern const ActorUpdateFn pickupUpdateTable[PICKUP_TYPE_COUNT];
	extern const ActorDrawFn pickupDrawTable[PICKUP_TYPE_COUNT];
}

DECLARE_ACTOR_EDITOR_DATA(pickup)