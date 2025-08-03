#pragma once
#include "asset_types.h"
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

ACTOR_EDITOR_DATA(Interactable,
	{ "checkpoint", GET_SUBTYPE_PROPERTIES(CheckpointData) },
	{ "npc", GET_SUBTYPE_PROPERTIES(NPCData) }
)