#pragma once
#include "typedef.h"

enum NPCSubtype : u16 {
	NPC_SUBTYPE_ENEMY_SLIME,
	NPC_SUBTYPE_ENEMY_SKULL,

	NPC_SUBTYPE_COUNT
};

struct NPCData {
	u16 health;

	u16 expValue;
	u16 lootType;
	u16 spawnOnDeath;
};

struct NPCState {
	u16 health;
	u16 damageCounter;
};

#ifdef EDITOR
constexpr const char* NPC_SUBTYPE_NAMES[NPC_SUBTYPE_COUNT] = { "Enemy Slime", "Enemy Skull" };
#endif

struct Actor;
struct PersistedActorData;

namespace Game {
	void InitializeNPC(Actor* pActor, const PersistedActorData& persistData);

	void NPCDie(Actor* pActor);
}