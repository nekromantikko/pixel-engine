#include "bullet.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

static void BulletDie(Actor* pBullet, const glm::vec2& effectPos) {
    pBullet->flags.pendingRemoval = true;
    Game::SpawnActor(pBullet->pPrototype->data.bulletData.deathEffect, effectPos);
}

static void HandleBulletEnemyCollision(Actor* pBullet, Actor* pEnemy) {
    if (pEnemy == nullptr) {
        return;
    }

    BulletDie(pBullet, pBullet->position);

    const u32 damage = Random::GenerateInt(1, 2);
    const u16 newHealth = Game::ActorTakeDamage(pEnemy, damage, pEnemy->state.enemyState.health, pEnemy->state.enemyState.damageCounter);
    if (newHealth == 0) {
        Game::EnemyDie(pEnemy);
    }
    pEnemy->state.enemyState.health = newHealth;
}

static void UpdateDefaultBullet(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pActor->position);
        return;
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    if (Game::ActorMoveVertical(pActor, hit)) {
        BulletDie(pActor, hit.impactPoint);
        return;
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    HandleBulletEnemyCollision(pActor, pEnemy);

    Game::GetAnimFrameFromDirection(pActor);
}

static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
    velocity = glm::reflect(velocity, normal);
    //Audio::PlaySFX(&ricochetSfx, CHAN_ID_PULSE0);
}

static void UpdateGrenade(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pActor->position);
        return;
    }

    constexpr r32 grenadeGravity = 0.04f;
    Game::ApplyGravity(pActor, grenadeGravity);

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    if (Game::ActorMoveVertical(pActor, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    HandleBulletEnemyCollision(pActor, pEnemy);

    Game::GetAnimFrameFromDirection(pActor);
}

static void InitializeBullet(Actor* pActor, const PersistedActorData* pPersistData) {
    pActor->state.bulletState.lifetimeCounter = pActor->pPrototype->data.bulletData.lifetime;
    pActor->drawState.layer = SPRITE_LAYER_FG;;
}

constexpr ActorInitFn Game::bulletInitTable[BULLET_TYPE_COUNT] = {
    InitializeBullet,
    InitializeBullet
};

constexpr ActorUpdateFn Game::bulletUpdateTable[BULLET_TYPE_COUNT] = {
    UpdateDefaultBullet,
    UpdateGrenade,
};

constexpr ActorDrawFn Game::bulletDrawTable[BULLET_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault,
};

#ifdef EDITOR
static const std::initializer_list<ActorEditorProperty> defaultProps = {
    {.name = "Lifetime", .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_U16, .components = 1, .offset = offsetof(BulletData, lifetime) },
    {.name = "Death effect", .type = ACTOR_EDITOR_PROPERTY_PROTOTYPE_INDEX, .offset = offsetof(BulletData, deathEffect) }
};

const ActorEditorData Editor::bulletEditorData = {
    { "Default", "Grenade" },
    { defaultProps, defaultProps },
};
#endif