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

enum ActorBehaviour {
	ACTOR_BEHAVIOUR_NONE,
	ACTOR_BEHAVIOUR_PLAYER_SIDESCROLLER,
	ACTOR_BEHAVIOUR_PLAYER_MAP,

	ACTOR_BEHAVIOUR_BULLET = 64,
	ACTOR_BEHAVIOUR_BULLET_BOUNCY,

	ACTOR_BEHAVIOUR_FIREBALL = 96,

	ACTOR_BEHAVIOUR_ENEMY_SLIME = 128,
	ACTOR_BEHAVIOUR_ENEMY_SKULL,

	ACTOR_BEHAVIOUR_FX_NUMBERS = 192,
	ACTOR_BEHAVIOUR_FX_EXPLOSION,

	ACTOR_BEHAVIOUR_COUNT
};

// Determines how to interpret anim frames
enum ActorAnimMode {
	ACTOR_ANIM_MODE_NONE = 0,
	ACTOR_ANIM_MODE_SPRITES,
	ACTOR_ANIM_MODE_METASPRITES,

	ACTOR_ANIM_MODE_COUNT
};

#ifdef EDITOR
constexpr const char* ACTOR_COLLISION_LAYER_NAMES[ACTOR_COLLISION_LAYER_COUNT] = { "None", "Player", "Projectile (friendly)", "Projectile (hostile)", "Enemy", "Pickup" };
constexpr const char* ACTOR_BEHAVIOUR_NAMES[ACTOR_BEHAVIOUR_COUNT] = { "None", "Player (Sidescroller)", "Player (Map)", "","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","",
																		"Bullet", "Bullet (Bouncy)", "","","","","","","","","","","","","","","","","","","","","","","","","","","","","","",
																		"Fireball","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","",
																		"Enemy Slime", "Enemy Skull","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","","",
																		"Fx Number", "Fx Explosion"};
constexpr const char* ACTOR_ANIM_MODE_NAMES[ACTOR_ANIM_MODE_COUNT] = { "None", "Sprites", "Metasprites" };
#endif

struct ActorAnimFrame {
	s32 spriteIndex;
	s32 metaspriteIndex;
};

#pragma region Actor types

enum ActorFacingDir : s8 {
	ACTOR_FACING_LEFT = -1, // 0b11
	ACTOR_FACING_RIGHT = 1 // 0b01
};

enum PlayerHeadFrame {
	PLAYER_HEAD_IDLE,
	PLAYER_HEAD_FWD,
	PLAYER_HEAD_FALL,
	PLAYER_HEAD_DMG
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
	PLAYER_WINGS_FLAP_END,

	PLAYER_WING_FRAME_COUNT
};

enum PlayerAimFrame {
	PLAYER_AIM_FWD,
	PLAYER_AIM_UP,
	PLAYER_AIM_DOWN
};

enum PlayerWeaponType {
	PLAYER_WEAPON_BOW,
	PLAYER_WEAPON_LAUNCHER
};

struct PlayerState {
	PlayerWeaponType weapon;
	u32 aimMode;
	s32 wingCounter;
	bool slowFall;
	bool doubleJumped;
	s32 shootCounter;
};

#pragma endregion

struct ActorPrototype {
	char name[ACTOR_PROTOTYPE_MAX_NAME_LENGTH];
	u32 collisionLayer;
	u32 behaviour;
	u32 animMode;

	u32 deathEffect;
	u32 unused0, unused1, unused2;

	AABB hitbox;

	// Different actor types can use this data how they see fit
	u32 frameCount;
	ActorAnimFrame frames[ACTOR_PROTOTYPE_MAX_FRAME_COUNT];
};

struct ActorFlags {
	s8 facingDir : 2;
	bool inAir : 1;
	bool active : 1;
	bool pendingRemoval : 1;
};

struct Actor {
	char name[ACTOR_MAX_NAME_LENGTH];

	ActorFlags flags;

	Vec2 initialPosition;
	Vec2 position;

	Vec2 velocity;
	r32 gravity = 0.01f;

	s32 health = 10;
	s32 damageCounter;

	s32 lifetime = 180;
	s32 lifetimeCounter;

	// TODO: Very specific, get rid of
	u32 drawNumber;

	// TODO: Define this in prototype!
	s32 animFrameLength = 6;
	s32 frameIndex = 0;
	s32 animCounter = 0;

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