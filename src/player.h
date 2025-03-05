#pragma once
#include "typedef.h"

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

struct PlayerFlags {
	u8 aimMode : 2;
	bool slowFall : 1;
	bool doubleJumped : 1;
	u8 sitState : 2;
};

struct PlayerState {
	PlayerFlags flags;

	u16 wingCounter;
	u16 wingFrame;
	u16 shootCounter;
	u16 damageCounter;
	u16 sitCounter;

	u16 entryDelayCounter;
	u16 deathCounter;
};

#ifdef EDITOR
constexpr const char* PLAYER_TYPE_NAMES[PLAYER_TYPE_COUNT] = { "Sidescroller", "Overworld" };
#endif
#pragma endregion

struct Actor;

namespace Game {
	extern const ActorInitFn playerInitTable[PLAYER_TYPE_COUNT];
	extern const ActorUpdateFn playerUpdateTable[PLAYER_TYPE_COUNT];
	extern const ActorDrawFn playerDrawTable[PLAYER_TYPE_COUNT];

	void HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy);
}