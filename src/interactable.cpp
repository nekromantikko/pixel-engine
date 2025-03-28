#include "interactable.h"
#include "actors.h"
#include "game_state.h"
#include "game_rendering.h"

static void UpdateCheckpoint(Actor* pActor, const ActorPrototypeNew* pPrototype) {
    if (pActor->state.checkpointState.activated) {
        pActor->drawState.animIndex = 1;
    }
    else {
        pActor->drawState.animIndex = 0;
    }
}

static void UpdateNPC(Actor* pActor, const ActorPrototypeNew* pPrototype) {

}

static void InitCheckpoint(Actor* pActor, const ActorPrototypeNew* pPrototype, const PersistedActorData* pPersistData) {
	if (pPersistData && pPersistData->activated) {
		pActor->state.checkpointState.activated = true;
	}
	pActor->drawState.layer = SPRITE_LAYER_BG;
}

static void InitNPC(Actor* pActor, const ActorPrototypeNew* pPrototype, const PersistedActorData* pPersistData) {

}

constexpr ActorInitFn Game::interactableInitTable[INTERACTABLE_TYPE_COUNT] = {
    InitCheckpoint,
    InitNPC,
};
constexpr ActorUpdateFn Game::interactableUpdateTable[INTERACTABLE_TYPE_COUNT] = {
    UpdateCheckpoint,
    UpdateNPC,
};
constexpr ActorDrawFn Game::interactableDrawTable[INTERACTABLE_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault
};

#ifdef EDITOR
const ActorEditorData Editor::interactableEditorData = {
    { "Checkpoint", "NPC" },
    { {}, {} }
};
#endif