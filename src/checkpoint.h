#pragma once
#include "typedef.h"

enum CheckpointSubtype : u16 {
	CHECKPOINT_SUBTYPE_DEFAULT,

	CHECKPOINT_SUBTYPE_COUNT
};

struct CheckpointData {

};

struct CheckpointState {
	bool activated;
};

#ifdef EDITOR
constexpr const char* CHECKPOINT_SUBTYPE_NAMES[CHECKPOINT_SUBTYPE_COUNT] = { "Default" };
#endif

struct Actor;
struct PersistedActorData;

namespace Game {
	void InitializeCheckpoint(Actor* pActor, const PersistedActorData& persistData);
}