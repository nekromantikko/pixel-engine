#pragma once

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

#pragma region NPC
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
#pragma endregion

#pragma region Bullets
enum PlayerBulletSubtype : u16 {
	BULLET_SUBTYPE_DEFAULT,
	BULLET_SUBTYPE_GRENADE,
	BULLET_SUBTYPE_FIREBALL,

	BULLET_SUBTYPE_COUNT
};

struct BulletData {
	u16 lifetime;
	u16 spawnOnDeath;
};

struct BulletState {
	u16 lifetime;
	u16 lifetimeCounter;
};

#ifdef EDITOR
constexpr const char* BULLET_SUBTYPE_NAMES[BULLET_SUBTYPE_COUNT] = { "Default", "Grenade", "Fireball" };
#endif
#pragma endregion

#pragma region Pickups
enum PickupSubtype : u16 {
	PICKUP_SUBTYPE_HALO,
	PICKUP_SUBTYPE_XP_REMNANT,

	PICKUP_SUBTYPE_COUNT
};

struct PickupData {
	s16 value;
};

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

#ifdef EDITOR
constexpr const char* PICKUP_SUBTYPE_NAMES[PICKUP_SUBTYPE_COUNT] = { "Halo (Exp) ", "Exp remnant" };
#endif
#pragma endregion

#pragma region Effects
enum EffectSubtype : u16 {
	EFFECT_SUBTYPE_NUMBERS,
	EFFECT_SUBTYPE_EXPLOSION,
	EFFECT_SUBTYPE_FEATHER,

	EFFECT_SUBTYPE_COUNT
};

struct EffectData {
	u16 lifetime;
};

struct EffectState {
	u16 lifetime;
	u16 lifetimeCounter;
	s16 value;
};

#ifdef EDITOR
constexpr const char* EFFECT_SUBTYPE_NAMES[EFFECT_SUBTYPE_COUNT] = { "Numbers", "Explosion", "Feather" };
#endif
#pragma endregion

#pragma region Checkpoints
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