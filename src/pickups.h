#pragma once
#include "typedef.h"

enum PickupSubtype : u16 {
	PICKUP_SUBTYPE_HALO,
	PICKUP_SUBTYPE_XP_REMNANT,

	PICKUP_SUBTYPE_COUNT
};

struct PickupData {
	s16 value;
};

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

#ifdef EDITOR
constexpr const char* PICKUP_SUBTYPE_NAMES[PICKUP_SUBTYPE_COUNT] = { "Halo (Exp) ", "Exp remnant" };
#endif

struct Actor;
struct PersistedActorData;

namespace Game {
	void InitializePickup(Actor* pActor, const PersistedActorData& persistData);
}