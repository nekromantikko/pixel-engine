#pragma once
#include "typedef.h"

enum CheckpointSubtype : TActorSubtype {
	INTERACTABLE_TYPE_CHECKPOINT,
	INTERACTABLE_TYPE_NPC,

	INTERACTABLE_TYPE_COUNT
};

struct CheckpointData {

};

struct CheckpointState {
	bool activated;
};

#ifdef EDITOR
constexpr const char* INTERACTABLE_TYPE_NAMES[INTERACTABLE_TYPE_COUNT] = { "Default" };
#endif

namespace Game {
	extern const ActorInitFn interactableInitTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorUpdateFn interactableUpdateTable[INTERACTABLE_TYPE_COUNT];
	extern const ActorDrawFn interactableDrawTable[INTERACTABLE_TYPE_COUNT];
}

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	extern const ActorEditorData interactableEditorData;
}
#endif