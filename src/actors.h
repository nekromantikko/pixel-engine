#pragma once
#include "typedef.h"
#include "collision.h"
#include "metasprite.h"

static constexpr u32 MAX_ACTOR_PROTOTYPE_COUNT = 256;
static constexpr u32 ACTOR_PROTOTYPE_MAX_NAME_LENGTH = 256;
static constexpr u32 ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT = 64;

static constexpr u32 ACTOR_MAX_NAME_LENGTH = 256;

#pragma region Common
enum ActorType : u16 {
	ACTOR_TYPE_PLAYER,
	ACTOR_TYPE_NPC,
	ACTOR_TYPE_BULLET,
	ACTOR_TYPE_PICKUP,
	ACTOR_TYPE_EFFECT,

	ACTOR_TYPE_COUNT
};

enum ActorFacingDir : s8 {
	ACTOR_FACING_LEFT = -1, // 0b11
	ACTOR_FACING_RIGHT = 1 // 0b01
};

enum ActorAlignment : u8 {
	ACTOR_ALIGNMENT_NEUTRAL,
	ACTOR_ALIGNMENT_FRIENDLY,
	ACTOR_ALIGNMENT_HOSTILE,

	ACTOR_ALIGNMENT_COUNT
};

struct ActorFlags {
	s8 facingDir : 2;
	bool inAir : 1;
	bool active : 1;
	bool pendingRemoval : 1;
};

#ifdef EDITOR
constexpr const char* ACTOR_TYPE_NAMES[ACTOR_TYPE_COUNT] = { "Player", "NPC", "Bullet", "Pickup", "Effect" };
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

struct PlayerFlags {
	u8 aimMode : 2;
	bool slowFall : 1;
	bool doubleJumped : 1;
};

struct PlayerState {
	PlayerFlags flags;

	u16 wingCounter;
	u16 wingFrame;
	u16 shootCounter;
	u16 damageCounter;
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
	PICKUP_SUBTYPE_COUNT
};

struct PickupData {

};

struct PickupState {

};

// #ifdef EDITOR
// constexpr const char* PICKUP_SUBTYPE_NAMES[PICKUP_SUBTYPE_COUNT] = { };
// #endif
#pragma endregion

#pragma region Effects
enum EffectSubtype : u16 {
	EFFECT_SUBTYPE_NUMBERS,
	EFFECT_SUBTYPE_EXPLOSION,

	EFFECT_SUBTYPE_COUNT
};

struct EffectData {
	u16 lifetime;
};

struct EffectState {
	u16 lifetime;
	u16 lifetimeCounter;
	u8 value;
};

#ifdef EDITOR
constexpr const char* EFFECT_SUBTYPE_NAMES[EFFECT_SUBTYPE_COUNT] = { "Numbers", "Explosion" };
#endif
#pragma endregion

enum AnimationType : u8 {
	ANIMATION_TYPE_SPRITES = 0,
	ANIMATION_TYPE_METASPRITES,

	ANIMATION_TYPE_COUNT
};

struct Animation {
	u8 type;
	u8 frameLength;
	u16 frameCount;
	s16 loopPoint;
	s16 metaspriteIndex;
};

#ifdef EDITOR
constexpr const char* ANIMATION_TYPE_NAMES[ANIMATION_TYPE_COUNT] = { "Sprites", "Metasprites" };
#endif

struct ActorPrototype {
	u16 type;
	u16 subtype;
	u8 alignment;

	AABB hitbox;

	u32 animCount;
	Animation animations[ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT];

	union {
		PlayerData playerData;
		NPCData npcData;
		BulletData bulletData;
		PickupData pickupData;
		EffectData effectData;
	};
};

struct Actor {
	char name[ACTOR_MAX_NAME_LENGTH];

	ActorFlags flags;

	glm::vec2 initialPosition;
	glm::vec2 position;

	glm::vec2 velocity;

	u16 frameIndex = 0;
	u16 animCounter = 0;

	union {
		PlayerState playerState;
		NPCState npcState;
		BulletState bulletState;
		PickupState pickupState;
		EffectState effectState;
	};

	const ActorPrototype* pPrototype;
};

namespace Actors {

	// Prototypes
	ActorPrototype* GetPrototype(s32 index);
	s32 GetPrototypeIndex(const ActorPrototype* pPrototype);
	char* GetPrototypeName(s32 index);
	char* GetPrototypeName(const ActorPrototype* pPrototype);
	void GetPrototypeNames(const char** pOutNames);

	void ClearPrototypes();
	void LoadPrototypes(const char* fname);
	void SavePrototypes(const char* fname);
}