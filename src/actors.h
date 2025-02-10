#pragma once
#include "typedef.h"
#include "collision.h"
#include "vector.h"
#include "metasprite.h"

static constexpr u32 MAX_ACTOR_PROTOTYPE_COUNT = 256;
static constexpr u32 ACTOR_PROTOTYPE_MAX_NAME_LENGTH = 256;
static constexpr u32 ACTOR_PROTOTYPE_MAX_FRAME_COUNT = 64;

static constexpr u32 ACTOR_MAX_NAME_LENGTH = 256;

// Determines how other actors will react to collision
enum ActorCollisionLayer {
	ACTOR_COLLISION_LAYER_NONE = 0,
	ACTOR_COLLISION_LAYER_PLAYER,
	ACTOR_COLLISION_LAYER_PROJECTILE_FRIENDLY,
	ACTOR_COLLISION_LAYER_PROJECTILE_HOSTILE,
	ACTOR_COLLISION_LAYER_ENEMY,
	ACTOR_COLLISION_LAYER_PICKUP,

	ACTOR_COLLISION_LAYER_COUNT
};

// TODO: This fills up quickly, maybe split into multiple sets of flags?
enum ActorBehaviourFlags {
	ACTOR_BEHAVIOUR_NONE = 0,
	ACTOR_BEHAVIOUR_RENDERABLE = 1 << 0,
	ACTOR_BEHAVIOUR_ANIM_DIR = 1 << 1,
	ACTOR_BEHAVIOUR_ANIM_LIFE = 1 << 2,
	ACTOR_BEHAVIOUR_FLAG_3 = 1 << 3,
	ACTOR_BEHAVIOUR_FLAG_4 = 1 << 4,
	ACTOR_BEHAVIOUR_FLAG_5 = 1 << 5,
	ACTOR_BEHAVIOUR_FLAG_6 = 1 << 6,
	ACTOR_BEHAVIOUR_FLAG_7 = 1 << 7,
	ACTOR_BEHAVIOUR_HEALTH = 1 << 8,
	ACTOR_BEHAVIOUR_LIFETIME = 1 << 9,
	ACTOR_BEHAVIOUR_FLAG_10 = 1 << 10,
	ACTOR_BEHAVIOUR_FLAG_11 = 1 << 11,
	ACTOR_BEHAVIOUR_FLAG_12 = 1 << 12,
	ACTOR_BEHAVIOUR_FLAG_13 = 1 << 13,
	ACTOR_BEHAVIOUR_FLAG_14 = 1 << 14,
	ACTOR_BEHAVIOUR_FLAG_15 = 1 << 15,
	ACTOR_BEHAVIOUR_GRAVITY = 1 << 16,
	ACTOR_BEHAVIOUR_BOUNCY = 1 << 17,
	ACTOR_BEHAVIOUR_FRAGILE = 1 << 18,
	ACTOR_BEHAVIOUR_EXPLODE = 1 << 19,
	ACTOR_BEHAVIOUR_FLAG_20 = 1 << 20,
	ACTOR_BEHAVIOUR_FLAG_21 = 1 << 21,
	ACTOR_BEHAVIOUR_FLAG_22 = 1 << 22,
	ACTOR_BEHAVIOUR_FLAG_23 = 1 << 23,
	ACTOR_BEHAVIOUR_NUMBERS = 1 << 24,
	ACTOR_BEHAVIOUR_FLAG_25 = 1 << 25,
	ACTOR_BEHAVIOUR_FLAG_26 = 1 << 26,
	ACTOR_BEHAVIOUR_FLAG_27 = 1 << 27,
	ACTOR_BEHAVIOUR_FLAG_28 = 1 << 28,
	ACTOR_BEHAVIOUR_FLAG_29 = 1 << 29,
	ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER = 1 << 30,
	ACTOR_BEHAVIOUR_PLAYER_MAP = 1 << 31,
};

constexpr u32 ACTOR_BEHAVIOUR_COUNT = 32;

// Determines how to interpret anim frames
enum ActorAnimMode {
	ACTOR_ANIM_MODE_NONE = 0,
	ACTOR_ANIM_MODE_SPRITES,
	ACTOR_ANIM_MODE_METASPRITES,

	ACTOR_ANIM_MODE_COUNT
};

#ifdef EDITOR
constexpr const char* ACTOR_COLLISION_LAYER_NAMES[ACTOR_COLLISION_LAYER_COUNT] = { "None", "Player", "Projectile (friendly)", "Projectile (hostile)", "Enemy", "Pickup" };
constexpr const char* ACTOR_BEHAVIOUR_NAMES[ACTOR_BEHAVIOUR_COUNT] = { "Renderable", "Animate Direction", "Animate Lifetime", "Flag 3", "Flag 4", "Flag 5", "Flag 6", "Flag 7", "Has Health", "Has Lifetime",
																		"Flag 10", "Flag 11", "Flag 12", "Flag 13", "Flag 14", "Flag 15", "Has Gravity", "Bouncy", "Fragile", "Explode",
																		"Flag 20", "Flag 21", "Flag 22", "Flag 23", "Draw Numbers", "Flag 25", "Flag 26", "Flag 27", "Flag 28", "Flag 29",
																		"Player (Sidescroller)", "Player (Map)" };
constexpr const char* ACTOR_ANIM_MODE_NAMES[ACTOR_ANIM_MODE_COUNT] = { "None", "Sprites", "Metasprites" };
#endif

struct ActorAnimFrame {
	s32 spriteIndex;
	s32 metaspriteIndex;
};

#pragma region Actor types

enum PlayerHeadFrame {
	PLAYER_HEAD_IDLE,
	PLAYER_HEAD_FWD,
	PLAYER_HEAD_FALL
};

enum PlayerLegsFrame {
	PLAYER_LEGS_IDLE,
	PLAYER_LEGS_FWD,
	PLAYER_LEGS_JUMP,
	PLAYER_LEGS_FALL
};

enum PlayerWingsFrame {
	PLAYER_WINGS_DESCEND,
	PLAYER_WINGS_FLAP_START,
	PLAYER_WINGS_ASCEND,
	PLAYER_WINGS_FLAP_END
};

enum PlayerAimFrame {
	PLAYER_AIM_FWD,
	PLAYER_AIM_UP,
	PLAYER_AIM_DOWN
};

enum Direction {
	DirLeft = -1,
	DirRight = 1
};

enum WeaponType {
	WpnBow,
	WpnLauncher
};

struct PlayerState {
	Direction direction = DirRight;
	WeaponType weapon;
	u32 aimMode;
	r32 wingCounter;
	u32 wingFrame;
	bool slowFall;
	bool inAir;
	bool doubleJumped;
	r32 shootCounter;
};

#pragma endregion

struct ActorPrototype {
	char name[ACTOR_PROTOTYPE_MAX_NAME_LENGTH];
	u32 collisionLayer;
	u32 behaviour;
	u32 animMode;

	AABB hitbox;

	// Different actor types can use this data how they see fit
	u32 frameCount;
	ActorAnimFrame frames[ACTOR_PROTOTYPE_MAX_FRAME_COUNT];
};

struct ActorDrawData {
	bool hFlip = false;
	bool vFlip = false;
	s32 paletteOverride = -1;
	IVec2 pixelOffset = { 0,0 };
	s32 frameIndex;
};

struct Actor {
	char name[ACTOR_MAX_NAME_LENGTH];

	Vec2 initialPosition;
	Vec2 position;

	Vec2 velocity;
	r32 gravity = 35;

	s32 health = 10;
	r32 damageCounter;

	r32 lifetime = 3;
	r32 lifetimeCounter;

	u32 drawNumber;
	ActorDrawData drawData;

	PlayerState playerState;

	const ActorPrototype* pPrototype;
};

namespace Actors {

	// Prototypes
	ActorPrototype* GetPrototype(s32 index);

	void ClearPrototypes();
	void LoadPrototypes(const char* fname);
	void SavePrototypes(const char* fname);
}