#pragma once
#include "typedef.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

#pragma region Player
enum PlayerType : TActorSubtype {
	PLAYER_TYPE_SIDESCROLLER,
	PLAYER_TYPE_OVERWORLD,

	PLAYER_TYPE_COUNT
};

enum PlayerWeaponType : u8 {
	PLAYER_WEAPON_BOW,
	PLAYER_WEAPON_LAUNCHER
};

enum PlayerModeBits : u8 {
	PLAYER_MODE_NORMAL,
	PLAYER_MODE_STAND_TO_SIT,
	PLAYER_MODE_SITTING,
	PLAYER_MODE_SIT_TO_STAND,
	PLAYER_MODE_DYING,
	PLAYER_MODE_DAMAGED,
	PLAYER_MODE_ENTERING,
	PLAYER_MODE_DODGE,
};

struct PlayerFlags {
	u8 aimMode : 2;
	bool slowFall : 1;
	bool doubleJumped : 1;
	bool airDodged : 1;
	u8 mode : 4;
};

struct PlayerState {
	PlayerFlags flags;

	u16 wingCounter;
	u16 wingFrame;
	u16 shootCounter;
	
	u16 modeTransitionCounter;
	u16 staminaRecoveryCounter;
};

struct Actor;
struct Damage;

namespace Game {
	extern const ActorInitFn playerInitTable[PLAYER_TYPE_COUNT];
	extern const ActorUpdateFn playerUpdateTable[PLAYER_TYPE_COUNT];
	extern const ActorDrawFn playerDrawTable[PLAYER_TYPE_COUNT];

	bool PlayerInvulnerable(Actor* pPlayer);
	void PlayerTakeDamage(Actor* pPlayer, const Damage& damage, const glm::vec2& enemyPos);
}

#ifdef EDITOR
#include "editor_actor.h"

namespace Editor {
	extern const ActorEditorData playerEditorData;
}
#endif