#include "actors.h"
#include "random.h"
#include "asset_manager.h"

static void InitSpawner(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    
}

static void UpdateExpSpawner(Actor* pActor, const ActorPrototype* pPrototype) {
    u16& remainingValue = pActor->data.expSpawner.remainingValue;
    if (remainingValue == 0) {
        pActor->flags.pendingRemoval = true;
        return;
    }

    const u16 largeExpValue = (AssetManager::GetAsset(pPrototype->data.expSpawner.large))->data.pickup.value;
    const u16 smallExpValue = (AssetManager::GetAsset(pPrototype->data.expSpawner.small))->data.pickup.value;

    u16 spawnedValue = remainingValue >= largeExpValue ? largeExpValue : smallExpValue;
    ActorPrototypeHandle prototypeHandle = spawnedValue >= largeExpValue ? pPrototype->data.expSpawner.large : pPrototype->data.expSpawner.small;

    const r32 speed = Random::GenerateReal(0.1f, 0.3f);
    const glm::vec2 velocity = Random::GenerateDirection() * speed;

    Actor* pSpawned = Game::SpawnActor(prototypeHandle, pActor->position, velocity);

    pSpawned->data.pickup.lingerCounter = 30;
    pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
    pSpawned->data.pickup.value = spawnedValue;

    if (remainingValue < spawnedValue) {
        remainingValue = 0;
    }
    else remainingValue -= spawnedValue;
}

static void UpdateEnemySpawner(Actor* pActor, const ActorPrototype* pPrototype) {

}

// Drops loot and deletes itself
static void UpdateLootSpawner(Actor* pActor, const ActorPrototype* pPrototype) {
    const LootSpawnerData& data = pPrototype->data.lootSpawner;

    for (u32 i = 0; i < data.typeCount; i++) {
        ActorPrototypeHandle lootType = data.types[i];
        const u8 spawnRate = data.spawnRates[i];

        if (Random::GenerateInt(0, 127) < spawnRate) {
            const r32 speed = Random::GenerateReal(0.1f, 0.3f);
            const glm::vec2 velocity = Random::GenerateDirection() * speed;

            Game::SpawnActor(lootType, pActor->position, velocity);
        }
    }

    pActor->flags.pendingRemoval = true;
}

static bool DrawNoOp(const Actor* pActor, const ActorPrototype* pPrototype) {
    return false;
}

constexpr ActorInitFn Game::spawnerInitTable[SPAWNER_TYPE_COUNT] = {
    InitSpawner,
    InitSpawner,
    InitSpawner,
};

constexpr ActorUpdateFn Game::spawnerUpdateTable[SPAWNER_TYPE_COUNT] = {
    UpdateExpSpawner,
    UpdateEnemySpawner,
    UpdateLootSpawner,
};

constexpr ActorDrawFn Game::spawnerDrawTable[SPAWNER_TYPE_COUNT] = {
    DrawNoOp,
    DrawNoOp,
    DrawNoOp,
};