#pragma once
#include "core_types.h"

#ifdef EDITOR
#include <vector>
#include "data_types.h"

enum ActorEditorPropertyType {
	ACTOR_EDITOR_PROPERTY_SCALAR,
	ACTOR_EDITOR_PROPERTY_ASSET
};

struct ActorEditorProperty {
	const char* name;
	ActorEditorPropertyType type;
	union {
		DataType dataType;
		AssetType assetType;
	};
	s32 components;
	u32 offset;
};

class ActorEditorData {
private:
	typedef std::pair<const char*, const std::vector<ActorEditorProperty>&> SubtypePropertyPair;

	std::vector<const char*> subtypeNames;
	std::vector<const std::vector<ActorEditorProperty>*> subtypeProperties;
public:
	ActorEditorData(std::vector<SubtypePropertyPair>&& subtypePropertyPairs)
		: subtypeNames(subtypePropertyPairs.size()), subtypeProperties(subtypePropertyPairs.size()) {

		for (size_t i = 0; i < subtypePropertyPairs.size(); ++i) {
			subtypeNames[i] = subtypePropertyPairs[i].first;
			subtypeProperties[i] = &subtypePropertyPairs[i].second;
		}
	}

	inline size_t GetSubtypeCount() const {
		return subtypeNames.size();
	}
	inline const char* const* GetSubtypeNames() const {
		return subtypeNames.data();
	}
	inline size_t GetPropertyCount(u32 subtype) const {
		return subtypeProperties[subtype]->size();
	}
	inline const ActorEditorProperty& GetProperty(u32 subtype, u32 index) const {
		return subtypeProperties[subtype]->at(index);
	}
};

#define ACTOR_SUBTYPE_PROPERTY_SCALAR(STRUCT_TYPE, FIELD, TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = DATA_TYPE_##TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_PROPERTY_ASSET(STRUCT_TYPE, FIELD, ASSET_TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_EDITOR_PROPERTY_ASSET, .assetType = ASSET_TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_PROPERTIES(STRUCT_TYPE, ...) \
	inline static const std::vector<ActorEditorProperty>& Get##STRUCT_TYPE##EditorProperties() { \
		static const std::vector<ActorEditorProperty> properties = { __VA_ARGS__ }; \
		return properties; \
	} \

#define GET_SUBTYPE_PROPERTIES(STRUCT_TYPE) \
		Get##STRUCT_TYPE##EditorProperties()

#define ACTOR_EDITOR_DATA(ACTOR_TYPE, ...) \
	inline static const ActorEditorData Get##ACTOR_TYPE##EditorData() { \
		static const ActorEditorData editorData = ActorEditorData({ __VA_ARGS__ }); \
		return editorData; \
	} \

#else
#define ACTOR_SUBTYPE_PROPERTY_SCALAR(STRUCT_TYPE, FIELD, TYPE, COMPONENTS)
#define ACTOR_SUBTYPE_PROPERTY_ASSET(STRUCT_TYPE, FIELD, ASSET_TYPE, COMPONENTS)
#define ACTOR_SUBTYPE_PROPERTIES(STRUCT_TYPE, ...)
#define GET_SUBTYPE_PROPERTIES(STRUCT_TYPE) \
		{}
#define ACTOR_EDITOR_DATA(ACTOR_TYPE, ...)
#endif

typedef u16 TActorType;
typedef u16 TActorSubtype;

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
	SoundHandle jumpSound;
	SoundHandle damageSound;
	SoundHandle gunSound; // TEMP!
};

ACTOR_SUBTYPE_PROPERTIES(PlayerData,
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, jumpSound, ASSET_TYPE_SOUND, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, damageSound, ASSET_TYPE_SOUND, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PlayerData, gunSound, ASSET_TYPE_SOUND, 1)
)

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

ACTOR_SUBTYPE_PROPERTIES(EnemyData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EnemyData, health, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EnemyData, expValue, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, lootSpawner, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, projectile, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EnemyData, expSpawner, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)

struct FireballData {
	u16 lifetime;
	ActorPrototypeHandle deathEffect;
};

ACTOR_SUBTYPE_PROPERTIES(FireballData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(FireballData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(FireballData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)

struct EnemyState {
	u16 health;
	u16 damageCounter;
};

struct FireballState {
	u16 lifetimeCounter;
};

ACTOR_EDITOR_DATA(Enemy,
	{ "slime", GET_SUBTYPE_PROPERTIES(EnemyData) },
	{ "skull", GET_SUBTYPE_PROPERTIES(EnemyData) },
	{ "fireball", GET_SUBTYPE_PROPERTIES(FireballData) }
)
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

ACTOR_SUBTYPE_PROPERTIES(BulletData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(BulletData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(BulletData, deathEffect, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
)

struct BulletState {
	u16 lifetimeCounter;
};

ACTOR_EDITOR_DATA(Bullet,
	{ "bullet", GET_SUBTYPE_PROPERTIES(BulletData) },
	{ "grenade", GET_SUBTYPE_PROPERTIES(BulletData) }
)
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

ACTOR_SUBTYPE_PROPERTIES(PickupData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(PickupData, value, S16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(PickupData, pickupSound, ASSET_TYPE_SOUND, 1)
)

struct PickupState {
	s16 value;
	u16 lingerCounter;
};

ACTOR_EDITOR_DATA(Pickup,
	{ "exp", GET_SUBTYPE_PROPERTIES(PickupData) },
	{ "exp_remnant", GET_SUBTYPE_PROPERTIES(PickupData) },
	{ "healing", GET_SUBTYPE_PROPERTIES(PickupData) }
)
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

ACTOR_SUBTYPE_PROPERTIES(EffectData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(EffectData, lifetime, U16, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(EffectData, sound, ASSET_TYPE_SOUND, 1)
);

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

ACTOR_EDITOR_DATA(Effect,
	{ "damage_numbers", GET_SUBTYPE_PROPERTIES(EffectData) },
	{ "explosion", GET_SUBTYPE_PROPERTIES(EffectData) },
	{ "feather", GET_SUBTYPE_PROPERTIES(EffectData) }
)
#pragma endregion

#pragma region Interactables
enum InteractableType : TActorSubtype {
	INTERACTABLE_TYPE_CHECKPOINT,
	INTERACTABLE_TYPE_NPC,

	INTERACTABLE_TYPE_COUNT
};

struct CheckpointData {

};

ACTOR_SUBTYPE_PROPERTIES(CheckpointData)

struct CheckpointState {
	bool activated;
};

struct NPCData {

};

ACTOR_SUBTYPE_PROPERTIES(NPCData)

ACTOR_EDITOR_DATA(Interactable,
	{ "checkpoint", GET_SUBTYPE_PROPERTIES(CheckpointData) },
	{ "npc", GET_SUBTYPE_PROPERTIES(NPCData) }
)
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

ACTOR_SUBTYPE_PROPERTIES(ExpSpawnerData,
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, large, ASSET_TYPE_ACTOR_PROTOTYPE, 1),
	ACTOR_SUBTYPE_PROPERTY_ASSET(ExpSpawnerData, small, ASSET_TYPE_ACTOR_PROTOTYPE, 1)
);

struct ExpSpawnerState {
	u16 remainingValue;
};

struct EnemySpawnerData {
};

ACTOR_SUBTYPE_PROPERTIES(EnemySpawnerData);

struct LootSpawnerData {
	u8 typeCount;
	u8 spawnRates[4];
	ActorPrototypeHandle types[4];
};

ACTOR_SUBTYPE_PROPERTIES(LootSpawnerData,
	ACTOR_SUBTYPE_PROPERTY_SCALAR(LootSpawnerData, typeCount, U8, 1),
	ACTOR_SUBTYPE_PROPERTY_SCALAR(LootSpawnerData, spawnRates, U8, 4),
	ACTOR_SUBTYPE_PROPERTY_ASSET(LootSpawnerData, types, ASSET_TYPE_ACTOR_PROTOTYPE, 4)
);

ACTOR_EDITOR_DATA(Spawner,
	{ "exp_spawner", GET_SUBTYPE_PROPERTIES(ExpSpawnerData) },
	{ "enemy_spawner", GET_SUBTYPE_PROPERTIES(EnemySpawnerData) },
	{ "loot_spawner", GET_SUBTYPE_PROPERTIES(LootSpawnerData) }
)
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

#ifdef EDITOR
namespace Editor {
	constexpr const char* actorTypeNames[ACTOR_TYPE_COUNT] = { "Player", "Enemy", "Bullet", "Pickup", "Effect", "Interactable", "Spawner" };

	inline static const ActorEditorData actorEditorData[ACTOR_TYPE_COUNT] = {
		GetPlayerEditorData(),
		GetEnemyEditorData(),
		GetBulletEditorData(),
		GetPickupEditorData(),
		GetEffectEditorData(),
		GetInteractableEditorData(),
		GetSpawnerEditorData(),
	};
}
#endif