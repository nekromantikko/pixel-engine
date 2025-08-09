#pragma once
#include "core_types.h"
#include "data_types.h"

typedef u16 TActorType;
typedef u16 TActorSubtype;

enum ActorPropertyType {
	ACTOR_PROPERTY_SCALAR,
	ACTOR_PROPERTY_ASSET
};

struct ActorProperty {
	const char* name;
	ActorPropertyType type;
	union {
		DataType dataType;
		AssetType assetType;
	};
	s32 components;
	u32 offset;
};

struct ActorTypeReflectionData {
	const size_t subtypeCount;
	const char* const* subtypeNames;
	const size_t* propertyCounts;
	const ActorProperty* const* subtypeProperties;
};

#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)                                    \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...)                         \
  macro(a1)                                                     \
  __VA_OPT__(FOR_EACH_AGAIN PARENS (macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

#define ACTOR_SUBTYPE_PROPERTY_SCALAR(STRUCT_TYPE, FIELD, TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_PROPERTY_SCALAR, .dataType = DATA_TYPE_##TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_PROPERTY_ASSET(STRUCT_TYPE, FIELD, ASSET_TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_PROPERTY_ASSET, .assetType = ASSET_TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_STRUCT_PROPERTIES(STRUCT_TYPE, ...) \
	constexpr ActorProperty __##STRUCT_TYPE##_STRUCT_PROPERTIES[] = { __VA_ARGS__ __VA_OPT__(,) {} }; \

#define ACTOR_SUBTYPE_REFLECTION_DATA(SUBTYPE, STRUCT_TYPE, NAME) \
	constexpr const char* __##SUBTYPE##_NAME = NAME; \
	constexpr const ActorProperty* __##SUBTYPE##_PROPERTIES = __##STRUCT_TYPE##_STRUCT_PROPERTIES; \
	constexpr size_t __##SUBTYPE##_PROPERTY_COUNT = sizeof(__##STRUCT_TYPE##_STRUCT_PROPERTIES) / sizeof(ActorProperty) - 1; \

#define GET_ACTOR_SUBTYPE_NAME(SUBTYPE) \
	__##SUBTYPE##_NAME,

#define GET_ACTOR_SUBTYPE_NAMES(...) \
{ \
	FOR_EACH(GET_ACTOR_SUBTYPE_NAME, __VA_ARGS__) \
}

#define GET_ACTOR_SUBTYPE_PROPERTIES_SINGLE(SUBTYPE) \
	__##SUBTYPE##_PROPERTIES,

#define GET_ACTOR_SUBTYPE_PROPERTIES(...) \
{ \
	FOR_EACH(GET_ACTOR_SUBTYPE_PROPERTIES_SINGLE, __VA_ARGS__) \
}

#define GET_ACTOR_SUBTYPE_PROPERTY_COUNT(SUBTYPE) \
	__##SUBTYPE##_PROPERTY_COUNT,

#define GET_ACTOR_SUBTYPE_PROPERTY_COUNTS(...) \
{ \
	FOR_EACH(GET_ACTOR_SUBTYPE_PROPERTY_COUNT, __VA_ARGS__) \
}

#define ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE, ...) \
	constexpr const char* __##ACTOR_TYPE##_SUBTYPE_NAMES[] = GET_ACTOR_SUBTYPE_NAMES(__VA_ARGS__); \
	constexpr size_t __##ACTOR_TYPE##_SUBTYPE_PROPERTY_COUNTS[] = GET_ACTOR_SUBTYPE_PROPERTY_COUNTS(__VA_ARGS__); \
	constexpr const ActorProperty* const __##ACTOR_TYPE##_SUBTYPE_PROPERTIES[] = GET_ACTOR_SUBTYPE_PROPERTIES(__VA_ARGS__); \
	constexpr ActorTypeReflectionData __##ACTOR_TYPE##_REFLECTION_DATA { \
		.subtypeCount = sizeof(__##ACTOR_TYPE##_SUBTYPE_NAMES) / sizeof(const char*), \
		.subtypeNames = __##ACTOR_TYPE##_SUBTYPE_NAMES, \
		.propertyCounts = __##ACTOR_TYPE##_SUBTYPE_PROPERTY_COUNTS, \
		.subtypeProperties = __##ACTOR_TYPE##_SUBTYPE_PROPERTIES \
	};

#define GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE) \
	__##ACTOR_TYPE##_REFLECTION_DATA

enum ActorType : TActorType {
	ACTOR_TYPE_PLAYER,
	ACTOR_TYPE_ENEMY,
	ACTOR_TYPE_BULLET,
	ACTOR_TYPE_PICKUP,
	ACTOR_TYPE_EFFECT,
	ACTOR_TYPE_INTERACTABLE,
	ACTOR_TYPE_SPAWNER,

	ACTOR_TYPE_COUNT
};

#pragma region Player
enum PlayerType : TActorSubtype {
	PLAYER_TYPE_SIDESCROLLER,
	PLAYER_TYPE_OVERWORLD,

	PLAYER_TYPE_COUNT
};

struct PlayerData {
	AnimationHandle wingAnimation;
	SoundHandle jumpSound;
	SoundHandle damageSound;
	SoundHandle gunSound; // TEMP!
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(PlayerData,
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, wingAnimation, ASSET_TYPE_ANIMATION, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, jumpSound, ASSET_TYPE_SOUND, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, damageSound, ASSET_TYPE_SOUND, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, gunSound, ASSET_TYPE_SOUND, 1)
)
ACTOR_SUBTYPE_REFLECTION_DATA(PLAYER_TYPE_SIDESCROLLER, PlayerData, "sidescroller")


struct PlayerOverworldData {

};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(PlayerOverworldData)
ACTOR_SUBTYPE_REFLECTION_DATA(PLAYER_TYPE_OVERWORLD, PlayerOverworldData, "overworld")

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

ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_PLAYER, PLAYER_TYPE_SIDESCROLLER, PLAYER_TYPE_OVERWORLD)
#pragma endregion

#pragma region Enemies
enum EnemyType : TActorSubtype {
	ENEMY_TYPE_SLIME,
	ENEMY_TYPE_SKULL,
	ENEMY_TYPE_FIREBALL,

	ENEMY_TYPE_COUNT
};

struct EnemyData {
	u16 health;

	u16 expValue;
	ActorPrototypeHandle lootSpawner;
	ActorPrototypeHandle deathEffect;
	ActorPrototypeHandle projectile;
	ActorPrototypeHandle expSpawner;
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(EnemyData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EnemyData, health, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EnemyData, expValue, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, lootSpawner, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, projectile, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, expSpawner, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)
ACTOR_SUBTYPE_REFLECTION_DATA(ENEMY_TYPE_SLIME, EnemyData, "slime")
ACTOR_SUBTYPE_REFLECTION_DATA(ENEMY_TYPE_SKULL, EnemyData, "skull")

struct FireballData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(FireballData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(FireballData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(FireballData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)
ACTOR_SUBTYPE_REFLECTION_DATA(ENEMY_TYPE_FIREBALL, FireballData, "fireball")

struct EnemyState {
	u16 health;
	u16 damageCounter;
};

struct FireballState {
	u16 lifetimeCounter;
};

ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_ENEMY, ENEMY_TYPE_SLIME, ENEMY_TYPE_SKULL, ENEMY_TYPE_FIREBALL)
#pragma endregion

#pragma region Bullets
enum BulletType : TActorSubtype {
	BULLET_TYPE_DEFAULT,
	BULLET_TYPE_GRENADE,

	BULLET_TYPE_COUNT
};

struct BulletData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(BulletData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(BulletData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(BulletData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)
ACTOR_SUBTYPE_REFLECTION_DATA(BULLET_TYPE_DEFAULT, BulletData, "default")
ACTOR_SUBTYPE_REFLECTION_DATA(BULLET_TYPE_GRENADE, BulletData, "grenade")

struct BulletState {
	u16 lifetimeCounter;
};

ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_BULLET, BULLET_TYPE_DEFAULT, BULLET_TYPE_GRENADE)
#pragma endregion

#pragma region Pickups
enum PickupType : TActorSubtype {
	PICKUP_TYPE_EXP,
	PICKUP_TYPE_EXP_REMNANT,
	PICKUP_TYPE_HEAL,

	PICKUP_TYPE_COUNT
};

struct PickupData {
	s16 value;
	SoundHandle pickupSound;
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(PickupData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(PickupData, value, S16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PickupData, pickupSound, ASSET_TYPE_SOUND, 1)
)
ACTOR_SUBTYPE_REFLECTION_DATA(PICKUP_TYPE_EXP, PickupData, "exp")
ACTOR_SUBTYPE_REFLECTION_DATA(PICKUP_TYPE_EXP_REMNANT, PickupData, "exp_remnant")
ACTOR_SUBTYPE_REFLECTION_DATA(PICKUP_TYPE_HEAL, PickupData, "healing")

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_PICKUP, PICKUP_TYPE_EXP, PICKUP_TYPE_EXP_REMNANT, PICKUP_TYPE_HEAL)
#pragma endregion

#pragma region Effects
enum EffectType : TActorSubtype {
	EFFECT_TYPE_NUMBERS,
	EFFECT_TYPE_EXPLOSION,
	EFFECT_TYPE_FEATHER,

	EFFECT_TYPE_COUNT
};

struct EffectData {
	u16 lifetime;
	SoundHandle sound;
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(EffectData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EffectData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EffectData, sound, ASSET_TYPE_SOUND, 1)
);
ACTOR_SUBTYPE_REFLECTION_DATA(EFFECT_TYPE_NUMBERS, EffectData, "damage_numbers")
ACTOR_SUBTYPE_REFLECTION_DATA(EFFECT_TYPE_EXPLOSION, EffectData, "explosion")
ACTOR_SUBTYPE_REFLECTION_DATA(EFFECT_TYPE_FEATHER, EffectData, "feather")

struct EffectState {
	u16 initialLifetime;
	u16 lifetimeCounter;
};

struct DamageFlags {
	bool healing : 1;
	bool crit : 1;
	u8 type : 3; // TODO: Add damage types?
};

struct Damage {
	u16 value;
	DamageFlags flags;
};

// Can be treated as EffectState
struct DamageNumberState {
	EffectState base;
	Damage damage;
};

ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_EFFECT, EFFECT_TYPE_NUMBERS, EFFECT_TYPE_EXPLOSION, EFFECT_TYPE_FEATHER)
#pragma endregion

#pragma region Interactables
enum InteractableType : TActorSubtype {
	INTERACTABLE_TYPE_CHECKPOINT,
	INTERACTABLE_TYPE_NPC,

	INTERACTABLE_TYPE_COUNT
};

struct CheckpointData {

};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(CheckpointData)
ACTOR_SUBTYPE_REFLECTION_DATA(INTERACTABLE_TYPE_CHECKPOINT, CheckpointData, "checkpoint")

struct CheckpointState {
	bool activated;
};

struct NPCData {

};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(NPCData)
ACTOR_SUBTYPE_REFLECTION_DATA(INTERACTABLE_TYPE_NPC, NPCData, "npc")

ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_INTERACTABLE, INTERACTABLE_TYPE_CHECKPOINT, INTERACTABLE_TYPE_NPC)
#pragma endregion

#pragma region Spawners
enum SpawnerType : TActorSubtype {
	SPAWNER_TYPE_EXP,
	SPAWNER_TYPE_ENEMY,
	SPAWNER_TYPE_LOOT,

	SPAWNER_TYPE_COUNT
};

struct ExpSpawnerData {
	ActorPrototypeHandle large;
	ActorPrototypeHandle small;
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(ExpSpawnerData,
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, large, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, small, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)
ACTOR_SUBTYPE_REFLECTION_DATA(SPAWNER_TYPE_EXP, ExpSpawnerData, "exp_spawner")

struct ExpSpawnerState {
	u16 remainingValue;
};

struct EnemySpawnerData {
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(EnemySpawnerData)
ACTOR_SUBTYPE_REFLECTION_DATA(SPAWNER_TYPE_ENEMY, EnemySpawnerData, "enemy_spawner")

struct LootSpawnerData {
	u8 typeCount;
	u8 spawnRates[4];
	ActorPrototypeHandle types[4];
};

ACTOR_SUBTYPE_STRUCT_PROPERTIES(LootSpawnerData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(LootSpawnerData, typeCount, U8, 1),
	ACTOR_SUBTYPE_PROPERTY_SCALAR(LootSpawnerData, spawnRates, U8, 4),
	ACTOR_SUBTYPE_PROPERTY_ASSET(LootSpawnerData, types, ASSET_TYPE_ACTOR_PROTOTYPE, 4)
)
ACTOR_SUBTYPE_REFLECTION_DATA(SPAWNER_TYPE_LOOT, LootSpawnerData, "loot_spawner")

ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_SPAWNER, SPAWNER_TYPE_EXP, SPAWNER_TYPE_ENEMY, SPAWNER_TYPE_LOOT)
#pragma endregion

union ActorPrototypeData {
	PlayerData playerData;
	EnemyData enemyData;
	FireballData fireballData;
	BulletData bulletData;
	PickupData pickupData;
	EffectData effectData;
	CheckpointData checkpointData;
	ExpSpawnerData expSpawner;
	LootSpawnerData lootSpawner;
};

union ActorState {
	PlayerState playerState;
	PlayerOverworldState playerOverworld;
	EnemyState enemyState;
	FireballState fireballState;
	BulletState bulletState;
	PickupState pickupState;
	EffectState effectState;
	DamageNumberState dmgNumberState;
	CheckpointState checkpointState;
	ExpSpawnerState expSpawner;
};

struct ActorPrototype {
	TActorType type;
	TActorSubtype subtype;

	AABB hitbox;

	ActorPrototypeData data;

	u32 animCount;
	u32 animOffset;

	inline AnimationHandle* GetAnimations() const {
		return (AnimationHandle*)((u8*)this + animOffset);
	}
};

constexpr const char* ACTOR_TYPE_NAMES[ACTOR_TYPE_COUNT] = { "Player", "Enemy", "Bullet", "Pickup", "Effect", "Interactable", "Spawner" };

constexpr ActorTypeReflectionData ACTOR_REFLECTION_DATA[ACTOR_TYPE_COUNT] = {
	GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_PLAYER),
	GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_ENEMY),
	GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_BULLET),
	GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_PICKUP),
	GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_EFFECT),
	GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_INTERACTABLE),
	GET_ACTOR_TYPE_REFLECTION_DATA(ACTOR_TYPE_SPAWNER),
};