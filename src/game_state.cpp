#include "game_state.h"
#include "game_rendering.h"
#include "game_input.h"
#include "game_ui.h"
#include "coroutines.h"
#include "level.h"

static GameData gameData;
static GameState state;

static Level* pCurrentLevel = nullptr;

// 16ms Frames elapsed while not paused
static u32 gameplayFramesElapsed = 0;
static bool freezeGameplay = false;

#pragma region Callbacks
static void ReviveDeadActor(u64 id, PersistedActorData& persistedData) {
    persistedData.dead = false;
}
#pragma endregion

#pragma region Gameplay
static void StepGameplayFrame(void (*tempCallback)(Actor* pActor)) {
	if (!freezeGameplay) {
        gameplayFramesElapsed++;
		
        Game::UpdateActors(tempCallback);
	}


    Game::Rendering::ClearSpriteLayers();
    Game::DrawActors();

    // Draw HUD
    Game::UI::DrawPlayerHealthBar(gameData.playerMaxHealth);
    Game::UI::DrawExpCounter();
}
#pragma endregion

#pragma region GameData
// Initializes game data to new game state
void Game::InitGameData() {
    // TODO: Come up with actual values
	gameData.playerMaxHealth = 96;
    SetPlayerHealth(gameData.playerMaxHealth);
    SetPlayerExp(0);
    SetPlayerWeapon(PLAYER_WEAPON_LAUNCHER);

	// TODO: Initialize first checkpoint

	gameData.expRemnant.levelIndex = -1;

	gameData.persistedActorData.Clear();
}
void Game::LoadGameData(u32 saveSlot) {
	// TODO: Load game data from save slot
}
void Game::SaveGameData(u32 saveSlot) {
	// TODO: Save game data to save slot
}
#pragma endregion

#pragma region Player
u16 Game::GetPlayerHealth() {
    return gameData.playerCurrentHealth;
}
u16 Game::GetPlayerMaxHealth() {
	return gameData.playerMaxHealth;
}
void Game::AddPlayerHealth(s32 health) {
    gameData.playerCurrentHealth += health;
	Game::UI::SetPlayerDisplayHealth(gameData.playerCurrentHealth);
}
void Game::SetPlayerHealth(u16 health) {
    gameData.playerCurrentHealth = health;
	Game::UI::SetPlayerDisplayHealth(gameData.playerCurrentHealth);
}
u16 Game::GetPlayerExp() {
    return gameData.playerExperience;
}
void Game::AddPlayerExp(s32 exp) {
	gameData.playerExperience += exp;
	Game::UI::SetPlayerDisplayExp(gameData.playerExperience);
}
void Game::SetPlayerExp(u16 exp) {
	gameData.playerExperience = exp;
	Game::UI::SetPlayerDisplayExp(gameData.playerExperience);
}
u16 Game::GetPlayerWeapon() {
    return gameData.playerWeapon;
}
void Game::SetPlayerWeapon(u16 weapon) {
    gameData.playerWeapon = weapon;
}
#pragma endregion

#pragma region ExpRemnant
void Game::ClearExpRemnant() {
	gameData.expRemnant.levelIndex = -1;
}
void Game::SetExpRemnant(s32 levelIndex, const glm::vec2& position, u16 value) {
	gameData.expRemnant.levelIndex = levelIndex;
	gameData.expRemnant.position = position;
	gameData.expRemnant.value = value;
}
#pragma endregion

#pragma region Checkpoint
Checkpoint Game::GetCheckpoint() {
    return gameData.checkpoint;
}
void Game::ActivateCheckpoint(const Actor* pCheckpoint) {
    if (pCheckpoint == nullptr) {
        return;
    }

    // Set checkpoint active
    PersistedActorData data = GetPersistedActorData(pCheckpoint->id);
    data.activated = true;
    SetPersistedActorData(pCheckpoint->id, data);

    Level* pCurrentLevel = GetCurrentLevel();
    // Set checkpoint data
    gameData.checkpoint = {
        .levelIndex = u16(Levels::GetIndex(pCurrentLevel)),
        .screenIndex = u8(Tiles::GetScreenIndex(pCheckpoint->position))
    };

    // Revive dead actors
    gameData.persistedActorData.ForEach(ReviveDeadActor);
}
#pragma endregion

#pragma region Persisted actor data
PersistedActorData Game::GetPersistedActorData(u64 id) {
    PersistedActorData* pPersistData = gameData.persistedActorData.Get(id);
    if (pPersistData == nullptr) {
        return PersistedActorData{};
    }
    return *pPersistData;
}
void Game::SetPersistedActorData(u64 id, const PersistedActorData& data) {
    PersistedActorData* pPersistData = gameData.persistedActorData.Get(id);
    if (pPersistData) {
        *pPersistData = data;
    }
    else {
        gameData.persistedActorData.Add(id, data);
    }
}
#pragma endregion

bool Game::LoadLevel(u32 index, s32 screenIndex, u8 direction, bool refresh) {
	Level* pLevel = Levels::GetLevel(index);
    if (pLevel == nullptr) {
        return false;
    }

    pCurrentLevel = pLevel;

    ReloadLevel(screenIndex, direction, refresh);
}

void Game::UnloadLevel(bool refresh) {
    if (pCurrentLevel == nullptr) {
        return;
    }

    Rendering::SetViewportPos(glm::vec2(0.0f), false);

	ClearActors();

    if (refresh) {
        Rendering::RefreshViewport();
    }
}

void Game::ReloadLevel(s32 screenIndex, u8 direction, bool refresh) {
    if (pCurrentLevel == nullptr) {
        return;
    }

    UnloadLevel(refresh);

    for (u32 i = 0; i < pCurrentLevel->actors.Count(); i++)
    {
        auto handle = pCurrentLevel->actors.GetHandle(i);
        const Actor* pActor = pCurrentLevel->actors.Get(handle);

        const PersistedActorData* pPersistData = gameData.persistedActorData.Get(pActor->id);
        if (!pPersistData || !(pPersistData->dead || pPersistData->permaDead)) {
            SpawnActor(pActor);
        }
    }

    // Spawn player in sidescrolling level
    if (pCurrentLevel->flags.type == LEVEL_TYPE_SIDESCROLLER) {
        //SpawnPlayerAtEntrance(pCurrentLevel, screenIndex, direction);
        //UpdateViewport();
    }

    // Spawn xp remnant
    if (gameData.expRemnant.levelIndex == Levels::GetIndex(pCurrentLevel)) {
        //Actor* pRemnant = SpawnActor(xpRemnantPrototypeIndex, gameData.expRemnant.position);
        //pRemnant->pickupState.value = gameData.expRemnant.value;
    }

    gameplayFramesElapsed = 0;

    if (refresh) {
        Rendering::RefreshViewport();
    }
}

Level* Game::GetCurrentLevel() {
    return pCurrentLevel;
}

u32 Game::GetFramesElapsed() {
	return gameplayFramesElapsed;
}

void Game::InitGameState(GameState initialState) {
    state = initialState;
}

void Game::StepFrame(void (*tempCallback)(Actor* pActor)) {
    Input::Update();
    StepCoroutines();

	switch (state) {
	case GAME_STATE_TITLE:
		break;
	case GAME_STATE_PLAYING:
		StepGameplayFrame(tempCallback);
        break;
	case GAME_STATE_PAUSED:
		break;
	case GAME_STATE_GAME_OVER:
		break;
	case GAME_STATE_CREDITS:
        break;
	case GAME_STATE_EXIT:
		break;
    default:
        break;
	}
}

void Game::FreezeGameplay() {
    freezeGameplay = true;
}
void Game::UnfreezeGameplay() {
    freezeGameplay = false;
}