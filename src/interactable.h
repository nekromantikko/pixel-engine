#pragma once
#include "interactable_data.h"

namespace Game {
	extern const ActorInitFn interactableInitTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorUpdateFn interactableUpdateTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorDrawFn interactableDrawTable[INTERACTABLE_TYPE_COUNT];
}