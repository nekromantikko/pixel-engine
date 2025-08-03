#pragma once
#include "asset_types.h"
#include "actor_reflection.h"
#define GLM_FORCE_RADIANS
#include <glm.hpp>

enum PlayerType : TActorSubtype {
	PLAYER_TYPE_SIDESCROLLER,
	PLAYER_TYPE_OVERWORLD,

	PLAYER_TYPE_COUNT
};

struct PlayerData {
	SoundHandle jumpSound;
	SoundHandle damageSound;
	SoundHandle gunSound; // TEMP!
};

ACTOR_SUBTYPE_PROPERTIES(PlayerData,
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, jumpSound, ASSET_TYPE_SOUND, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, damageSound, ASSET_TYPE_SOUND, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, gunSound, ASSET_TYPE_SOUND, 1)
);

struct PlayerOverworldData {

};

ACTOR_SUBTYPE_PROPERTIES(PlayerOverworldData)

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

struct PlayerOverworldState {
	glm::ivec2 facingDir;
	u16 movementCounter;
};

ACTOR_EDITOR_DATA(Player,
	{ "sidescroller", GET_SUBTYPE_PROPERTIES(PlayerData) },
	{ "overworld", GET_SUBTYPE_PROPERTIES(PlayerOverworldData) }
)