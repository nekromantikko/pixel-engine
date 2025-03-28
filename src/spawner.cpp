#include "spawner.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "random.h"
#include "asset_manager.h"

static void InitSpawner(Actor* pActor, const ActorPrototypeNew* pPrototype, const PersistedActorData* pPersistData) {
    
}

static void UpdateExpSpawner(Actor* pActor, const ActorPrototypeNew* pPrototype) {
    u16& remainingValue = pActor->state.expSpawner.remainingValue;
    if (remainingValue == 0) {
        pActor->flags.pendingRemoval = true;
        return;
    }

    const u16 largeExpValue = ((ActorPrototypeNew*)AssetManager::GetAsset(pPrototype->data.expSpawner.large))->data.pickupData.value;
    const u16 smallExpValue = ((ActorPrototypeNew*)AssetManager::GetAsset(pPrototype->data.expSpawner.small))->data.pickupData.value;

    u16 spawnedValue = remainingValue >= largeExpValue ? largeExpValue : smallExpValue;
    ActorPrototypeHandle prototypeId = spawnedValue >= largeExpValue ? pPrototype->data.expSpawner.large : pPrototype->data.expSpawner.small;

    const r32 speed = Random::GenerateReal(0.1f, 0.3f);
    const glm::vec2 velocity = Random::GenerateDirection() * speed;

    Actor* pSpawned = Game::SpawnActor(prototypeId, pActor->position, velocity);

    pSpawned->state.pickupState.lingerCounter = 30;
    pSpawned->flags.facingDir = (s8)Random::GenerateInt(-1, 1);
    pSpawned->state.pickupState.value = pPrototype->data.pickupData.value;

    if (remainingValue < spawnedValue) {
        remainingValue = 0;
    }
    else remainingValue -= spawnedValue;
}

static void UpdateEnemySpawner(Actor* pActor, const ActorPrototypeNew* pPrototype) {

}

// Drops loot and deletes itself
static void UpdateLootSpawner(Actor* pActor, const ActorPrototypeNew* pPrototype) {
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

static bool DrawNoOp(const Actor* pActor, const ActorPrototypeNew* pPrototype) {
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

#ifdef EDITOR
static const std::initializer_list<ActorEditorProperty> expSpawnerProps = {
    {.name = "Large exp", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(ExpSpawnerData, large) },
    {.name = "Small exp", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(ExpSpawnerData, small) }
};

static const std::initializer_list<ActorEditorProperty> lootSpawnerProps = {
    {.name = "Type count", .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_U8, .components = 1, .offset = offsetof(LootSpawnerData, typeCount) },
    {.name = "Spawn rates", .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_U8, .components = 4, .offset = offsetof(LootSpawnerData, spawnRates) },
    {.name = "Loot type 0", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(LootSpawnerData, types[0])},
    {.name = "Loot type 1", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(LootSpawnerData, types[1])},
    {.name = "Loot type 2", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(LootSpawnerData, types[2])},
    {.name = "Loot type 3", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(LootSpawnerData, types[3])},
};

const ActorEditorData Editor::spawnerEditorData = {
    { "Exp spawner", "Enemy spawner", "Loot spawner" },
    { expSpawnerProps, {}, lootSpawnerProps },
};
#endif