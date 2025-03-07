#include "game_state.h"
#include "game_rendering.h"
#include "game_input.h"
#include "game_ui.h"
#include "coroutines.h"
#include "dialog.h"
#include "level.h"
#include "collision.h"
#include "actor_prototypes.h"

// TODO: Define in editor in game settings 
constexpr s32 playerPrototypeIndex = 0;
constexpr s32 xpRemnantPrototypeIndex = 0x0c;

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
static void CorrectPlayerSpawnY(const Level* pLevel, Actor* pPlayer) {
    HitResult hit{};

    const r32 dy = VIEWPORT_HEIGHT_METATILES / 2.0f;  // Sweep downwards to find a floor

    Collision::SweepBoxVertical(pLevel->pTilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
    while (hit.startPenetrating) {
        pPlayer->position.y -= 1.0f;
        Collision::SweepBoxVertical(pLevel->pTilemap, pPlayer->pPrototype->hitbox, pPlayer->position, dy, hit);
    }
    pPlayer->position = hit.location;
}

static bool ActorIsCheckpoint(const Actor* pActor) {
    return pActor->pPrototype->type == ACTOR_TYPE_INTERACTABLE && pActor->pPrototype->subtype == INTERACTABLE_TYPE_CHECKPOINT;
}

static bool SpawnPlayerAtCheckpoint() {
    Actor* pCheckpoint = Game::GetFirstActor(ActorIsCheckpoint);
    if (pCheckpoint == nullptr) {
        return false;
    }

    Actor* pPlayer = Game::SpawnActor(playerPrototypeIndex, pCheckpoint->position);
    if (pPlayer) {
        pPlayer->state.playerState.flags.mode = PLAYER_MODE_SITTING;
        return true;
    }

    return false;
}

static bool SpawnPlayerAtEntrance(const Level* pLevel, u8 screenIndex, u8 direction) {
    if (direction == SCREEN_EXIT_DIR_DEATH_WARP) {
        // Restore life
        Game::SetPlayerHealth(Game::GetPlayerMaxHealth());

        return SpawnPlayerAtCheckpoint();
    }

    r32 x = (screenIndex % TILEMAP_MAX_DIM_SCREENS) * VIEWPORT_WIDTH_METATILES;
    r32 y = (screenIndex / TILEMAP_MAX_DIM_SCREENS) * VIEWPORT_HEIGHT_METATILES;

    Actor* pPlayer = Game::SpawnActor(playerPrototypeIndex, glm::vec2(x, y));
    if (pPlayer == nullptr) {
        return false;
    }

    constexpr r32 initialHSpeed = 0.0625f;

    switch (direction) {
    case SCREEN_EXIT_DIR_LEFT: {
        pPlayer->position.x += 0.5f;
        pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
        pPlayer->flags.facingDir = ACTOR_FACING_RIGHT;
        pPlayer->velocity.x = initialHSpeed;
        CorrectPlayerSpawnY(pLevel, pPlayer);
        break;
    }
    case SCREEN_EXIT_DIR_RIGHT: {
        pPlayer->position.x += VIEWPORT_WIDTH_METATILES - 0.5f;
        pPlayer->position.y += VIEWPORT_HEIGHT_METATILES / 2.0f;
        pPlayer->flags.facingDir = ACTOR_FACING_LEFT;
        pPlayer->velocity.x = -initialHSpeed;
        CorrectPlayerSpawnY(pLevel, pPlayer);
        break;
    }
    case SCREEN_EXIT_DIR_TOP: {
        pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
        pPlayer->position.y += 0.5f;
        pPlayer->velocity.y = 0.25f;
        break;
    }
    case SCREEN_EXIT_DIR_BOTTOM: {
        pPlayer->position.x += VIEWPORT_WIDTH_METATILES / 2.0f;
        pPlayer->position.y += VIEWPORT_HEIGHT_METATILES - 0.5f;
        pPlayer->velocity.y = -0.25f;
        break;
    }
    default:
        break;
    }

    pPlayer->state.playerState.flags.mode = PLAYER_MODE_ENTERING;
    pPlayer->state.playerState.modeTransitionCounter = 15;

    return true;
}

static void ViewportFollowPlayer() {
    Actor* pPlayer = Game::GetPlayer();
    if (!pPlayer) {
        return;
    }

    constexpr glm::vec2 viewportScrollThreshold = { 4.0f, 3.0f };

    const glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    const glm::vec2 viewportCenter = viewportPos + glm::vec2{ VIEWPORT_WIDTH_METATILES / 2.0f, VIEWPORT_HEIGHT_METATILES / 2.0f };
    const glm::vec2 targetOffset = pPlayer->position - viewportCenter;

    glm::vec2 delta = { 0.0f, 0.0f };
    if (targetOffset.x > viewportScrollThreshold.x) {
        delta.x = targetOffset.x - viewportScrollThreshold.x;
    }
    else if (targetOffset.x < -viewportScrollThreshold.x) {
        delta.x = targetOffset.x + viewportScrollThreshold.x;
    }

    if (targetOffset.y > viewportScrollThreshold.y) {
        delta.y = targetOffset.y - viewportScrollThreshold.y;
    }
    else if (targetOffset.y < -viewportScrollThreshold.y) {
        delta.y = targetOffset.y + viewportScrollThreshold.y;
    }

    Game::Rendering::SetViewportPos(viewportPos + delta);
}

static void StepGameplayFrame() {
	if (!freezeGameplay) {
        gameplayFramesElapsed++;
		
        Game::UpdateActors();
        ViewportFollowPlayer();
        Game::UI::Update();
	}

    Game::Rendering::ClearSpriteLayers();
    Game::DrawActors();

    // Draw HUD
    Game::UI::DrawPlayerHealthBar(gameData.playerMaxHealth);
    Game::UI::DrawPlayerStaminaBar(gameData.playerMaxStamina);
    Game::UI::DrawExpCounter();
}
#pragma endregion

#pragma region Coroutines
struct ScreenShakeState {
    const s16 magnitude;
    u16 duration;
};

static bool ShakeScreenCoroutine(void* userData) {
    ScreenShakeState& state = *(ScreenShakeState*)userData;

    if (state.duration == 0) {
        freezeGameplay = false;
        return false;
    }

    const r32 magnitudeMetatiles = r32(state.magnitude) / METATILE_DIM_PIXELS;
    glm::vec2 viewportPos = Game::Rendering::GetViewportPos();
    viewportPos.x += Random::GenerateReal(-magnitudeMetatiles, magnitudeMetatiles);
    viewportPos.y += Random::GenerateReal(-magnitudeMetatiles, magnitudeMetatiles);

    Game::Rendering::SetViewportPos(viewportPos);

    state.duration--;
    return true;
}

enum LevelTransitionStatus : u8 {
    TRANSITION_FADE_OUT,
    TRANSITION_LOADING,
    TRANSITION_FADE_IN,
    TRANSITION_COMPLETE
};

struct LevelTransitionState {
    u16 nextLevelIndex;
    u16 nextScreenIndex;
    u8 nextDirection;

    r32 progress = 0.0f;
    u8 status = TRANSITION_FADE_OUT;
    u8 holdTimer = 12;
};

static void UpdateFadeToBlack(r32 progress) {
    progress = glm::smoothstep(0.0f, 1.0f, progress);

    u8 colors[PALETTE_COLOR_COUNT];
    for (u32 i = 0; i < PALETTE_COUNT; i++) {
        Game::Rendering::GetPalettePresetColors(i, colors);
        for (u32 j = 0; j < PALETTE_COLOR_COUNT; j++) {
            u8 color = colors[j];

            const s32 baseBrightness = (color & 0b1110000) >> 4;
            const s32 hue = color & 0b0001111;

            const s32 minBrightness = hue == 0 ? 0 : -1;

            s32 newBrightness = glm::mix(minBrightness, baseBrightness, 1.0f - progress);

            if (newBrightness >= 0) {
                colors[j] = hue | (newBrightness << 4);
            }
            else {
                colors[j] = 0x00;
            }
        }
        Game::Rendering::WritePaletteColors(i, colors);
    }
}

static bool LevelTransitionCoroutine(void* userData) {
    LevelTransitionState* state = (LevelTransitionState*)userData;

    switch (state->status) {
    case TRANSITION_FADE_OUT: {
        if (state->progress < 1.0f) {
            state->progress += 0.1f;
            UpdateFadeToBlack(state->progress);
            return true;
        }
        state->status = TRANSITION_LOADING;
        break;
    }
    case TRANSITION_LOADING: {
        if (state->holdTimer > 0) {
            state->holdTimer--;
            return true;
        }
        Game::LoadLevel(state->nextLevelIndex, state->nextScreenIndex, state->nextDirection);
        state->status = TRANSITION_FADE_IN;
        freezeGameplay = false;
        break;
    }
    case TRANSITION_FADE_IN: {
        if (state->progress > 0.0f) {
            state->progress -= 0.10f;
            UpdateFadeToBlack(state->progress);
            return true;
        }
        state->status = TRANSITION_COMPLETE;
        break;
    }
    default:
        return false;
    }

    return true;
}
#pragma endregion

#pragma region GameData
// Initializes game data to new game state
void Game::InitGameData() {
    // TODO: Come up with actual values
	gameData.playerMaxHealth = 96;
    SetPlayerHealth(gameData.playerMaxHealth);
    gameData.playerMaxStamina = 64;
    SetPlayerStamina(gameData.playerMaxStamina);
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
s16 Game::GetPlayerHealth() {
    return gameData.playerCurrentHealth;
}
s16 Game::GetPlayerMaxHealth() {
	return gameData.playerMaxHealth;
}
void Game::AddPlayerHealth(s16 health) {
    gameData.playerCurrentHealth += health;
    gameData.playerCurrentHealth = glm::clamp(gameData.playerCurrentHealth, s16(0), gameData.playerMaxHealth);
	Game::UI::SetPlayerDisplayHealth(gameData.playerCurrentHealth);
}
void Game::SetPlayerHealth(s16 health) {
    gameData.playerCurrentHealth = health;
    gameData.playerCurrentHealth = glm::clamp(gameData.playerCurrentHealth, s16(0), gameData.playerMaxHealth);
	Game::UI::SetPlayerDisplayHealth(gameData.playerCurrentHealth);
}
s16 Game::GetPlayerStamina() {
    return gameData.playerCurrentStamina;
}

s16 Game::GetPlayerMaxStamina() {
    return gameData.playerMaxStamina;
}

void Game::AddPlayerStamina(s16 stamina) {
    gameData.playerCurrentStamina += stamina;
    gameData.playerCurrentStamina = glm::clamp(gameData.playerCurrentStamina, s16(0), gameData.playerMaxStamina);
    Game::UI::SetPlayerDisplayStamina(gameData.playerCurrentStamina);
}

void Game::SetPlayerStamina(s16 stamina) {
    gameData.playerCurrentStamina = stamina;
    gameData.playerCurrentStamina = glm::clamp(gameData.playerCurrentStamina, s16(0), gameData.playerMaxStamina);
    Game::UI::SetPlayerDisplayStamina(gameData.playerCurrentStamina);
}
s16 Game::GetPlayerExp() {
    return gameData.playerExperience;
}
void Game::AddPlayerExp(s16 exp) {
	gameData.playerExperience += exp;
    gameData.playerExperience = glm::clamp(gameData.playerExperience, s16(0), s16(SHRT_MAX));
	Game::UI::SetPlayerDisplayExp(gameData.playerExperience);
}
void Game::SetPlayerExp(s16 exp) {
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
    PersistedActorData* pData = GetPersistedActorData(pCheckpoint->persistId);
    if (pData) {
        pData->activated = true;
    }
    else SetPersistedActorData(pCheckpoint->persistId, { .activated = true });

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
PersistedActorData* Game::GetPersistedActorData(u64 id) {
    if (id == UUID_NULL) {
        return nullptr;
    }

    PersistedActorData* pPersistData = gameData.persistedActorData.Get(id);
    return pPersistData;
}
void Game::SetPersistedActorData(u64 id, const PersistedActorData& data) {
    if (id == UUID_NULL) {
        return;
    }

    PersistedActorData* pPersistData = gameData.persistedActorData.Get(id);
    if (pPersistData) {
        *pPersistData = data;
    }
    else {
        gameData.persistedActorData.Add(id, data);
    }
}
#pragma endregion

#pragma region Level
bool Game::LoadLevel(u32 index, s32 screenIndex, u8 direction, bool refresh) {
	Level* pLevel = Levels::GetLevel(index);
    if (pLevel == nullptr) {
        return false;
    }

    pCurrentLevel = pLevel;

    return ReloadLevel(screenIndex, direction, refresh);
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

bool Game::ReloadLevel(s32 screenIndex, u8 direction, bool refresh) {
    if (pCurrentLevel == nullptr) {
        return false;
    }

    UnloadLevel(refresh);

    for (u32 i = 0; i < pCurrentLevel->actors.Count(); i++)
    {
        auto handle = pCurrentLevel->actors.GetHandle(i);
        const Actor* pActor = pCurrentLevel->actors.Get(handle);

        const PersistedActorData* pPersistData = gameData.persistedActorData.Get(pActor->persistId);
        if (!pPersistData || !(pPersistData->dead || pPersistData->permaDead)) {
            SpawnActor(pActor);
        }
    }

    // Spawn player in sidescrolling level
    if (pCurrentLevel->flags.type == LEVEL_TYPE_SIDESCROLLER) {
        SpawnPlayerAtEntrance(pCurrentLevel, screenIndex, direction);
        ViewportFollowPlayer();
    }

    // Spawn xp remnant
    if (gameData.expRemnant.levelIndex == Levels::GetIndex(pCurrentLevel)) {
        Actor* pRemnant = SpawnActor(xpRemnantPrototypeIndex, gameData.expRemnant.position);
        pRemnant->state.pickupState.value = gameData.expRemnant.value;
    }

    gameplayFramesElapsed = 0;

    if (refresh) {
        Rendering::RefreshViewport();
    }

    return true;
}

Level* Game::GetCurrentLevel() {
    return pCurrentLevel;
}

u32 Game::GetFramesElapsed() {
	return gameplayFramesElapsed;
}
#pragma endregion

void Game::InitGameState(GameState initialState) {
    state = initialState;
}

void Game::StepFrame() {
    Input::Update();
    StepCoroutines();
    UpdateDialog();

	switch (state) {
	case GAME_STATE_TITLE:
		break;
	case GAME_STATE_PLAYING:
		StepGameplayFrame();
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

void Game::TriggerScreenShake(s16 magnitude, u16 duration, bool freeze) {
    freezeGameplay |= freeze;
    ScreenShakeState state = {
        .magnitude = magnitude,
        .duration = duration
    };
    StartCoroutine(ShakeScreenCoroutine, state);
}

void Game::TriggerLevelTransition(u16 targetLevelIndex, u16 targetScreenIndex, u8 enterDirection, void(*callback)()) {
    LevelTransitionState state = {
            .nextLevelIndex = targetLevelIndex,
            .nextScreenIndex = targetScreenIndex,
            .nextDirection = enterDirection,
    };
    StartCoroutine(LevelTransitionCoroutine, state, callback);
    freezeGameplay = true;
}