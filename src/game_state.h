#pragma once
#include "typedef.h"
#include "fixed_hash_map.h"
#include "asset_types.h"

struct Checkpoint {
    DungeonHandle dungeonId;
    glm::i8vec2 gridOffset;
};

struct ExpRemnant {
    DungeonHandle dungeonId = DungeonHandle::Null();
    glm::i8vec2 gridOffset;

    glm::vec2 position;
    u16 value;
};

struct PersistedActorData {
    bool dead : 1 = false;
    bool permaDead : 1 = false;
    bool activated : 1 = false;
};

struct GameData {
    s16 playerCurrentHealth;
    s16 playerMaxHealth;

    s16 playerCurrentStamina;
    s16 playerMaxStamina;

    s16 playerExperience;
    s16 playerWeapon;

	Checkpoint checkpoint;
    ExpRemnant expRemnant;
    FixedHashMap<PersistedActorData> persistedActorData;
};

enum GameState {
    GAME_STATE_NONE,
    GAME_STATE_TITLE,
    GAME_STATE_OVERWORLD,
	GAME_STATE_DUNGEON,
	GAME_STATE_DUNGEON_MAP,
	GAME_STATE_GAME_OVER,
	GAME_STATE_CREDITS,
	GAME_STATE_EXIT,
};

struct Actor;
struct Dungeon;
struct RoomTemplate;
struct RoomInstance;
struct Tilemap;
struct Overworld;

namespace Game {
    void InitGameData();
    void LoadGameData(u32 saveSlot);
	void SaveGameData(u32 saveSlot);

    s16 GetPlayerHealth();
	s16 GetPlayerMaxHealth();
	void AddPlayerHealth(s16 health);
	void SetPlayerHealth(s16 health);
    s16 GetPlayerStamina();
    s16 GetPlayerMaxStamina();
    void AddPlayerStamina(s16 stamina);
    void SetPlayerStamina(s16 stamina);
    s16 GetPlayerExp();
	void AddPlayerExp(s16 exp);
	void SetPlayerExp(s16 exp);
    u16 GetPlayerWeapon();
    void SetPlayerWeapon(u16 weapon);

	void ClearExpRemnant();
	void SetExpRemnant(const glm::vec2& position, u16 value);

    Checkpoint GetCheckpoint();
    void SetCheckpoint(const Checkpoint& checkpoint);
    void ActivateCheckpoint(const Actor* pCheckpoint);

    PersistedActorData* GetPersistedActorData(u64 id);
    void SetPersistedActorData(u64 id, const PersistedActorData& data);

    const Overworld* GetOverworld();
    bool LoadOverworld(u8 keyAreaIndex, u8 direction);
    void EnterOverworldArea(u8 keyAreaIndex, const glm::ivec2& direction);

    bool LoadRoom(DungeonHandle dungeonId, const glm::i8vec2 gridCell, u8 direction = 0);
    void UnloadRoom();
    bool ReloadRoom(const glm::i8vec2 screenOffset = { 0,0 }, u8 direction = 0);
    DungeonHandle GetCurrentDungeon();
    glm::i8vec2 GetCurrentRoomOffset();
    const RoomInstance* GetCurrentRoom();
    glm::i8vec2 GetDungeonGridCell(const glm::vec2& worldPos);
    void DiscoverScreen(const glm::i8vec2 gridCell);

    glm::ivec2 GetCurrentPlayAreaSize();
    const Tilemap* GetCurrentTilemap();
    
    void DestroyTileAt(const glm::ivec2& tileCoord, const glm::vec2& impactPoint);

	u32 GetFramesElapsed();

    void InitGameState(GameState initialState);
    void StepFrame();

    void TriggerScreenShake(s16 magnitude, u16 duration, bool freezeGameplay);
    void TriggerLevelTransition(DungeonHandle targetDungeon, glm::i8vec2 targetGridCell, u8 enterDirection, void (*callback)() = nullptr);
}