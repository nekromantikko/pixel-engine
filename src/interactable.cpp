#include "actors.h"
#include "game_state.h"
#include "game_rendering.h"

static void UpdateCheckpoint(Actor* pActor, const ActorPrototype* pPrototype) {
    if (pActor->state.checkpointState.activated) {
        pActor->drawState.animIndex = 1;
    }
    else {
        pActor->drawState.animIndex = 0;
    }
}

static void UpdateNPC(Actor* pActor, const ActorPrototype* pPrototype) {

}

static void UpdateTreasureChest(Actor* pActor, const ActorPrototype* pPrototype) {
    // Update damage counter for visual feedback
    Game::UpdateCounter(pActor->state.treasureChestState.damageCounter);
    Game::SetDamagePaletteOverride(pActor, pActor->state.treasureChestState.damageCounter);

    // Update animation based on state
    if (pActor->state.treasureChestState.opened) {
        pActor->drawState.animIndex = 1; // Opened animation
    } else {
        pActor->drawState.animIndex = 0; // Closed animation
    }

    Game::AdvanceCurrentAnimation(pActor, pPrototype);
}

static void InitCheckpoint(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
	if (pPersistData && pPersistData->activated) {
		pActor->state.checkpointState.activated = true;
	}
	pActor->drawState.layer = SPRITE_LAYER_BG;
}

static void InitNPC(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {

}

static void InitTreasureChest(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pActor->state.treasureChestState.health = pPrototype->data.treasureChestData.health;
    pActor->state.treasureChestState.damageCounter = 0;
    pActor->state.treasureChestState.opened = false;
    
    // Check if chest was already opened from persistent data
    if (pPersistData && pPersistData->activated) {
        pActor->state.treasureChestState.opened = true;
        pActor->state.treasureChestState.health = 0;
    }
    
    pActor->drawState.layer = SPRITE_LAYER_BG;
}

constexpr ActorInitFn Game::interactableInitTable[INTERACTABLE_TYPE_COUNT] = {
    InitCheckpoint,
    InitNPC,
    InitTreasureChest,
};
constexpr ActorUpdateFn Game::interactableUpdateTable[INTERACTABLE_TYPE_COUNT] = {
    UpdateCheckpoint,
    UpdateNPC,
    UpdateTreasureChest,
};
constexpr ActorDrawFn Game::interactableDrawTable[INTERACTABLE_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault,
    Game::DrawActorDefault
};