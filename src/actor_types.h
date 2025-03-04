#pragma once
#include "player.h"
#include "npc.h"
#include "bullets.h"
#include "pickups.h"
#include "effects.h"
#include "checkpoint.h"

#pragma region Common
enum ActorType : u16 {
	ACTOR_TYPE_PLAYER,
	ACTOR_TYPE_NPC,
	ACTOR_TYPE_BULLET,
	ACTOR_TYPE_PICKUP,
	ACTOR_TYPE_EFFECT,
	ACTOR_TYPE_CHECKPOINT,

	ACTOR_TYPE_COUNT
};

enum ActorAlignment : u8 {
	ACTOR_ALIGNMENT_NEUTRAL,
	ACTOR_ALIGNMENT_FRIENDLY,
	ACTOR_ALIGNMENT_HOSTILE,

	ACTOR_ALIGNMENT_COUNT
};

#ifdef EDITOR
constexpr const char* ACTOR_TYPE_NAMES[ACTOR_TYPE_COUNT] = { "Player", "NPC", "Bullet", "Pickup", "Effect", "Checkpoint" };
constexpr const char* ACTOR_ALIGNMENT_NAMES[ACTOR_ALIGNMENT_COUNT] = { "Neutral", "Friendly", "Hostile" };
#endif
#pragma endregion

// TODO: Would be nice to be able to autogenerate these!
union ActorPrototypeData {
	PlayerData playerData;
	NPCData npcData;
	BulletData bulletData;
	PickupData pickupData;
	EffectData effectData;
	CheckpointData checkpointData;
};

union ActorState {
	PlayerState playerState;
	NPCState npcState;
	BulletState bulletState;
	PickupState pickupState;
	EffectState effectState;
	CheckpointState checkpointState;
};