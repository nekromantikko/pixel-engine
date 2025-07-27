#pragma once
#include "typedef.h"
#include "actor_reflection.h"

enum CheckpointSubtype : TActorSubtype {
	INTERACTABLE_TYPE_CHECKPOINT,
	INTERACTABLE_TYPE_NPC,

	INTERACTABLE_TYPE_COUNT
};

struct CheckpointData {

};

ACTOR_SUBTYPE_PROPERTIES(CheckpointData)

struct CheckpointState {
	bool activated;
};

struct NPCData {

};

ACTOR_SUBTYPE_PROPERTIES(NPCData)

#ifdef EDITOR
constexpr const char* INTERACTABLE_TYPE_NAMES[INTERACTABLE_TYPE_COUNT] = { "Default" };
#endif

namespace Game {
	extern const ActorInitFn interactableInitTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorUpdateFn interactableUpdateTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorDrawFn interactableDrawTable[INTERACTABLE_TYPE_COUNT];
}

DECLARE_ACTOR_EDITOR_DATA(interactable)