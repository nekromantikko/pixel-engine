#pragma once
#include "player_data.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

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

struct Actor;
struct Damage;

namespace Game {
	extern const ActorInitFn playerInitTable[PLAYER_TYPE_COUNT];
	extern const ActorUpdateFn playerUpdateTable[PLAYER_TYPE_COUNT];
	extern const ActorDrawFn playerDrawTable[PLAYER_TYPE_COUNT];

	bool PlayerInvulnerable(Actor* pPlayer);
	void PlayerTakeDamage(Actor* pPlayer, const Damage& damage, const glm::vec2& enemyPos);
}