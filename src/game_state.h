#pragma once
#include "typedef.h"
#include "fixed_hash_map.h"

struct Checkpoint {
    u16 levelIndex;
    u8 screenIndex;
};

struct ExpRemnant {
    s32 levelIndex = -1;
    glm::vec2 position;
    u16 value;
};

struct PersistedActorData {
    bool dead : 1 = false;
    bool permaDead : 1 = false;
    bool activated : 1 = false;
};

struct GameData {
    u16 playerCurrentHealth;
    u16 playerMaxHealth;
    u16 playerExperience;
    u16 playerWeapon;

	Checkpoint checkpoint;
    ExpRemnant expRemnant;
    FixedHashMap<PersistedActorData> persistedActorData;
};

enum GameState {
    GAME_STATE_NONE,
	GAME_STATE_TITLE,
	GAME_STATE_PLAYING,
	GAME_STATE_PAUSED,
	GAME_STATE_GAME_OVER,
	GAME_STATE_CREDITS,
	GAME_STATE_EXIT,
};

struct Level;
struct Actor;

namespace Game {
    void InitGameData();
    void LoadGameData(u32 saveSlot);
	void SaveGameData(u32 saveSlot);

    u16 GetPlayerHealth();
	u16 GetPlayerMaxHealth();
	void AddPlayerHealth(s32 health);
	void SetPlayerHealth(u16 health);
    u16 GetPlayerExp();
	void AddPlayerExp(s32 exp);
	void SetPlayerExp(u16 exp);
    u16 GetPlayerWeapon();
    void SetPlayerWeapon(u16 weapon);

	void ClearExpRemnant();
	void SetExpRemnant(s32 levelIndex, const glm::vec2& position, u16 value);

    Checkpoint GetCheckpoint();
    void SetCheckpoint(const Checkpoint& checkpoint);
    void ActivateCheckpoint(const Actor* pCheckpoint);

    PersistedActorData GetPersistedActorData(u64 id);
    void SetPersistedActorData(u64 id, const PersistedActorData& data);

    bool LoadLevel(u32 index, s32 screenIndex = 0, u8 direction = 0, bool refresh = true);
    void UnloadLevel(bool refresh = true);
    void ReloadLevel(s32 screenIndex = 0, u8 direction = 0, bool refresh = true);
	Level* GetCurrentLevel();

	u32 GetFramesElapsed();

    void InitGameState(GameState initialState);
    void StepFrame(void (*tempCallback)(Actor* pActor));

    void FreezeGameplay();
    void UnfreezeGameplay();
}