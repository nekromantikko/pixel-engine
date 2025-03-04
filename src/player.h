#pragma once
#include "typedef.h"

#pragma region Player
enum PlayerSubtype : u16 {
	PLAYER_SUBTYPE_SIDESCROLLER,
	PLAYER_SUBTYPE_MAP,

	PLAYER_SUBTYPE_COUNT
};

struct PlayerData {

};

enum PlayerHeadFrame : u8 {
	PLAYER_HEAD_IDLE,
	PLAYER_HEAD_FWD,
	PLAYER_HEAD_FALL,
	PLAYER_HEAD_DMG
};

enum PlayerLegsFrame : u8 {
	PLAYER_LEGS_IDLE,
	PLAYER_LEGS_FWD,
	PLAYER_LEGS_JUMP,
	PLAYER_LEGS_FALL
};

enum PlayerWingsFrame : u8 {
	PLAYER_WINGS_DESCEND,
	PLAYER_WINGS_FLAP_START,
	PLAYER_WINGS_ASCEND,
	PLAYER_WINGS_FLAP_END,

	PLAYER_WING_FRAME_COUNT
};

enum PlayerAimFrame : u8 {
	PLAYER_AIM_FWD,
	PLAYER_AIM_UP,
	PLAYER_AIM_DOWN
};

enum PlayerWeaponType : u8 {
	PLAYER_WEAPON_BOW,
	PLAYER_WEAPON_LAUNCHER
};

enum PlayerSitDownState : u8 {
	PLAYER_STANDING = 0b00,
	PLAYER_SITTING = 0b01,
	PLAYER_SIT_TO_STAND = 0b10,
	PLAYER_STAND_TO_SIT = 0b11
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
constexpr const char* PLAYER_SUBTYPE_NAMES[PLAYER_SUBTYPE_COUNT] = { "Sidescroller", "Map" };
#endif
#pragma endregion

struct Actor;
struct PersistedActorData;

namespace Game {
	void InitializePlayer(Actor* pPlayer, const PersistedActorData& persistData);
	void UpdatePlayer(Actor* pPlayer);
	void DrawPlayer(const Actor* pPlayer);

	void HandlePlayerEnemyCollision(Actor* pPlayer, Actor* pEnemy);
}