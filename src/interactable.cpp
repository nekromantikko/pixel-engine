#include "interactable.h"
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

static void InitCheckpoint(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
	if (pPersistData && pPersistData->activated) {
		pActor->state.checkpointState.activated = true;
	}
	pActor->drawState.layer = SPRITE_LAYER_BG;
}

static void InitNPC(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {

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

DEFINE_ACTOR_EDITOR_DATA(interactable,
	{ "checkpoint", GET_SUBTYPE_PROPERTIES(CheckpointData) },
	{ "npc", GET_SUBTYPE_PROPERTIES(NPCData) }
);