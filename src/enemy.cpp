#include "enemy.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

// TODO: Should be determined by enemy stats
constexpr u16 baseDamage = 10;

static void UpdateSlimeEnemy(Actor* pActor) {
    Game::UpdateCounter(pActor->state.enemyState.damageCounter);

    if (!pActor->flags.inAir) {
        const bool shouldJump = Random::GenerateInt(0, 127) == 0;
        if (shouldJump) {
            pActor->velocity.y = -0.25f;
            Game::ActorFacePlayer(pActor);
            pActor->velocity.x = 0.15625f * pActor->flags.facingDir;
        }
        else {
            pActor->velocity.x = 0.00625f * pActor->flags.facingDir;
        }
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        pActor->velocity.x = 0.0f;
        pActor->flags.facingDir = (s8)hit.impactNormal.x;
    }

    Game::ApplyGravity(pActor);

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, hit)) {
        pActor->velocity.y = 0.0f;

        if (hit.impactNormal.y < 0.0f) {
            pActor->flags.inAir = false;
        }
    }

    Actor* pPlayer = Game::GetPlayer();
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
    }

    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pActor, pActor->state.enemyState.damageCounter);
}

static void UpdateSkullEnemy(Actor* pActor) {
    Game::UpdateCounter(pActor->state.enemyState.damageCounter);

    Game::ActorFacePlayer(pActor);

    static const r32 amplitude = 4.0f;
    const r32 sineTime = glm::sin(Game::GetFramesElapsed() / 60.f);
    pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

    // Shoot fireballs
    const bool shouldFire = Random::GenerateInt(0, 127) == 0;
    if (shouldFire) {

        Actor* pPlayer = Game::GetPlayer();
        if (pPlayer != nullptr) {
            const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
            const glm::vec2 velocity = playerDir * 0.0625f;

            Game::SpawnActor(pActor->pPrototype->data.enemyData.projectile, pActor->position, velocity);
        }
    }


    Actor* pPlayer = Game::GetPlayer();
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
    }

    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pActor, pActor->state.enemyState.damageCounter);
}

static void FireballDie(Actor* pActor, const glm::vec2& effectPos) {
    pActor->flags.pendingRemoval = true;
    Game::SpawnActor(pActor->pPrototype->data.fireballData.deathEffect, effectPos);
}

static void UpdateFireball(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.fireballState.lifetimeCounter)) {
        return FireballDie(pActor, pActor->position);
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        return FireballDie(pActor, hit.impactPoint);

    }

    if (Game::ActorMoveVertical(pActor, hit)) {
        return FireballDie(pActor, hit.impactPoint);
    }

    Actor* pPlayer = Game::GetPlayer();
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
        return FireballDie(pActor, pActor->position);
    }

    Game::AdvanceCurrentAnimation(pActor);
}

static void InitEnemy(Actor* pActor, const PersistedActorData* pPersistData) {
    pActor->state.enemyState.health = pActor->pPrototype->data.enemyData.health;
    pActor->state.enemyState.damageCounter = 0;
    pActor->drawState.layer = SPRITE_LAYER_FG;
}

#pragma region Public API
void Game::EnemyDie(Actor* pActor) {
    pActor->flags.pendingRemoval = true;

    PersistedActorData* pPersistData = GetPersistedActorData(pActor->persistId);
    if (pPersistData) {
        pPersistData->dead = true;
    }
    else SetPersistedActorData(pActor->persistId, { .dead = true });

    //Audio::PlaySFX(&enemyDieSfx, CHAN_ID_NOISE);
    SpawnActor(pActor->pPrototype->data.enemyData.deathEffect, pActor->position);

    // Spawn exp halos
    const u16 totalExpValue = pActor->pPrototype->data.enemyData.expValue;
    Actor* pExpSpawner = SpawnActor(pActor->pPrototype->data.enemyData.expSpawner, pActor->position);
    pExpSpawner->state.expSpawner.remainingValue = totalExpValue;

    // Spawn loot
    SpawnActor(pActor->pPrototype->data.enemyData.lootSpawner, pActor->position);
}
#pragma endregion

constexpr ActorInitFn Game::enemyInitTable[ENEMY_TYPE_COUNT] = {
    InitEnemy,
    InitEnemy,
    InitEnemy,
};
constexpr ActorUpdateFn Game::enemyUpdateTable[ENEMY_TYPE_COUNT] = {
    UpdateSlimeEnemy,
    UpdateSkullEnemy,
    UpdateFireball,
};
constexpr ActorDrawFn Game::enemyDrawTable[ENEMY_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault,
    Game::DrawActorDefault,
};

#ifdef EDITOR
static const std::initializer_list<ActorEditorProperty> defaultProps = {
    { .name = "Health", .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_U16, .components = 1, .offset = offsetof(EnemyData, health) },
    { .name = "Exp value", .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_U16, .components = 1, .offset = offsetof(EnemyData, expValue) },
    { .name = "Death effect", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(EnemyData, deathEffect) },
    { .name = "Projectile", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(EnemyData, projectile) },
    { .name = "Exp spawner", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(EnemyData, expSpawner) },
    { .name = "Loot spawner", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(EnemyData, lootSpawner) },
};

static const std::initializer_list<ActorEditorProperty> fireballProps = {
    {.name = "Lifetime", .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_U16, .components = 1, .offset = offsetof(FireballData, lifetime) },
    {.name = "Death effect", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(FireballData, deathEffect) },
};

const ActorEditorData Editor::enemyEditorData(
    { "Enemy Slime", "Enemy Skull", "Fireball" },
    { defaultProps, defaultProps, fireballProps }
);
#endif