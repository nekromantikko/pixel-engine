#pragma once
#include "typedef.h"
#include "asset_types.h"

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

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

namespace Game {
	extern const ActorInitFn pickupInitTable[PICKUP_TYPE_COUNT];
	extern const ActorUpdateFn pickupUpdateTable[PICKUP_TYPE_COUNT];
	extern const ActorDrawFn pickupDrawTable[PICKUP_TYPE_COUNT];
}

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	extern const ActorEditorData pickupEditorData;
}
#endif