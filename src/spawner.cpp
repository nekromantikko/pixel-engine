#include "spawner.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "random.h"

static void InitSpawner(Actor* pActor, const PersistedActorData* pPersistData) {
    
}

static void UpdateExpSpawner(Actor* pActor) {
    u16& remainingValue = pActor->state.expSpawner.remainingValue;
    if (remainingValue == 0) {
        pActor->flags.pendingRemoval = true;
        return;
    }

    const u16 largeExpValue = Assets::GetActorPrototype(pActor->pPrototype->data.expSpawner.large)->data.pickupData.value;
    const u16 smallExpValue = Assets::GetActorPrototype(pActor->pPrototype->data.expSpawner.small)->data.pickupData.value;

    u16 spawnedValue = remainingValue >= largeExpValue ? largeExpValue : smallExpValue;
    s32 prototypeIndex = spawnedValue >= largeExpValue ? pActor->pPrototype->data.expSpawner.large : pActor->pPrototype->data.expSpawner.small;

    const r32 speed = Random::GenerateReal(0.1f, 0.3f);
    const glm::vec2 velocity = Random::GenerateDirection() * speed;

    Actor* pSpawned = Game::SpawnActor(prototypeIndex, pActor->position, velocity);

    pSpawned->state.pickupState.lingerCounter = 30;
    pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
    pSpawned->state.pickupState.value = pSpawned->pPrototype->data.pickupData.value;

    if (remainingValue < spawnedValue) {
        remainingValue = 0;
    }
    else remainingValue -= spawnedValue;
}

static void UpdateEnemySpawner(Actor* pActor) {

}

static bool DrawNoOp(const Actor* pActor) {
    return false;
}

constexpr ActorInitFn Game::spawnerInitTable[SPAWNER_TYPE_COUNT] = {
    InitSpawner,
    InitSpawner
};

constexpr ActorUpdateFn Game::spawnerUpdateTable[SPAWNER_TYPE_COUNT] = {
    UpdateExpSpawner,
    UpdateEnemySpawner,
};

constexpr ActorDrawFn Game::spawnerDrawTable[SPAWNER_TYPE_COUNT] = {
    DrawNoOp,
    DrawNoOp,
};

#ifdef EDITOR
static const std::initializer_list<ActorEditorProperty> expSpawnerProps = {
    {.name = "Large exp", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(ExpSpawnerData, large) },
    {.name = "Small exp", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(ExpSpawnerData, small) }
};

const ActorEditorData Editor::spawnerEditorData = {
    { "Exp spawner", "Enemy spawner" },
    { expSpawnerProps, {} },
};
#endif