#include "checkpoint.h"
#include "actors.h"
#include "game_state.h"
#include "game_rendering.h"

static void UpdateCheckpoint(Actor* pActor) {
    if (pActor->state.checkpointState.activated) {
        pActor->drawState.animIndex = 1;
    }
    else {
        pActor->drawState.animIndex = 0;
    }
}

#pragma region Public API
void Game::InitializeCheckpoint(Actor* pActor, const PersistedActorData& persistData) {
	if (persistData.activated) {
		pActor->state.checkpointState.activated = true;
	}
	pActor->drawState.layer = SPRITE_LAYER_BG;

    pActor->pUpdateFn = UpdateCheckpoint;
    pActor->pDrawFn = nullptr;
}
#pragma endregion