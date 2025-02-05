#pragma once
#include "typedef.h"
#include "collision.h"
#include "vector.h"
#include "metasprite.h"

static constexpr u32 MAX_ACTOR_PRESET_COUNT = 256;
static constexpr u32 ACTOR_MAX_NAME_LENGTH = 256;
static constexpr u32 ACTOR_STATE_SIZE = 256;

// Determines how other actors will react
enum ActorType {
	ACTOR_TYPE_NONE = 0,
	ACTOR_TYPE_PLAYER,
	ACTOR_TYPE_PROJECTILE_FRIENDLY,
	ACTOR_TYPE_PROJECTILE_HOSTILE,
	ACTOR_TYPE_ENEMY,
	ACTOR_TYPE_PICKUP,

	ACTOR_TYPE_COUNT
};

// Determines how the actor gets updated
enum ActorBehaviour {
	ACTOR_BEHAVIOUR_NONE = 0,
	ACTOR_BEHAVIOUR_PLAYER,
	ACTOR_BEHAVIOUR_ENEMY_SKULL,
	ACTOR_BEHAVIOUR_GRENADE,
	ACTOR_BEHAVIOUR_BULLET,
	ACTOR_BEHAVIOUR_COUNT
};

#ifdef EDITOR
constexpr const char* ACTOR_TYPE_NAMES[ACTOR_TYPE_COUNT] = { "None", "Player", "Projectile (friendly)", "Projectile (hostile)", "Enemy", "Pickup" };
constexpr const char* ACTOR_BEHAVIOUR_NAMES[ACTOR_BEHAVIOUR_COUNT] = { "None", "Player", "Skull Enemy", "Bouncy Grenade", "Bullet" };
#endif

#pragma region Actor types

enum HeadMode {
	HeadIdle,
	HeadFwd,
	HeadFall
};

enum LegsMode {
	LegsIdle,
	LegsFwd,
	LegsJump,
	LegsFall
};

enum WingMode {
	WingFlap,
	WingJump,
	WingFall
};

enum AimMode {
	AimFwd,
	AimUp,
	AimDown
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
	Vec2 velocity;
	Direction direction;
	WeaponType weapon;
	HeadMode hMode;
	LegsMode lMode;
	WingMode wMode;
	AimMode aMode;
	r32 wingCounter;
	u32 wingFrame;
	s32 vOffset;
	bool slowFall;
	bool inAir;
	bool doubleJumped;
	r32 shootTimer = 0;
	r32 damageTimer;
};

struct GrenadeState {
	Vec2 velocity;
	u32 bounces;
};

struct EnemyState {
	r32 baseHeight;
	s32 health;
	r32 damageTimer;
};

#pragma endregion

struct ActorPreset {
	u32 type;
	u32 behaviour;

	Hitbox hitbox;

	// TODO: Animation of some kind?
	Metasprite* pMetasprite;
};

struct Actor {
	Vec2 position;

	u8 state[ACTOR_STATE_SIZE];

	const ActorPreset* pPreset;
};

namespace Actors {

	// Presets
	ActorPreset* GetPreset(s32 index);
	char* GetPresetName(s32 index);

	void ClearPresets();
	void LoadPresets(const char* fname);
	void SavePresets(const char* fname);
}