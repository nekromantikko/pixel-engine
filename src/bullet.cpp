#include "bullet.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

static void BulletDie(Actor* pBullet, const ActorPrototype* pPrototype, const glm::vec2& effectPos) {
    pBullet->flags.pendingRemoval = true;
    Game::SpawnActor(pPrototype->data.bulletData.deathEffect, effectPos);
}

static void HandleBulletEnemyCollision(Actor* pBullet, const ActorPrototype* pPrototype, Actor* pEnemy) {
    const ActorPrototype* pEnemyPrototype = Game::GetActorPrototype(pEnemy);

    if (pEnemy == nullptr || pEnemyPrototype->subtype == ENEMY_TYPE_FIREBALL) {
        return;
    }

    BulletDie(pBullet, pPrototype, pBullet->position);

    // TODO: Use value from weapon data
    constexpr u16 baseDamage = 1;
	const Damage damage = Game::CalculateDamage(pEnemy, baseDamage);

    const u16 newHealth = Game::ActorTakeDamage(pEnemy, damage, pEnemy->state.enemyState.health, pEnemy->state.enemyState.damageCounter);
    if (newHealth == 0) {
        Game::EnemyDie(pEnemy, pEnemyPrototype);
    }
    pEnemy->state.enemyState.health = newHealth;
}

static void UpdateDefaultBullet(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pPrototype, pActor->position);
        return;
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        BulletDie(pActor, pPrototype, hit.impactPoint);
        return;
    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        BulletDie(pActor, pPrototype, hit.impactPoint);
        return;
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    if (pEnemy) {
        HandleBulletEnemyCollision(pActor, pPrototype, pEnemy);
    }

    Game::GetAnimFrameFromDirection(pActor, pPrototype);
}

static void BulletRicochet(glm::vec2& velocity, const glm::vec2& normal) {
    velocity = glm::reflect(velocity, normal);
}

static void UpdateGrenade(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.bulletState.lifetimeCounter)) {
        BulletDie(pActor, pPrototype, pActor->position);
        return;
    }

    constexpr r32 grenadeGravity = 0.04f;
    Game::ApplyGravity(pActor, grenadeGravity);

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        BulletRicochet(pActor->velocity, hit.impactNormal);
    }

    Actor* pEnemy = Game::GetFirstActorCollision(pActor, ACTOR_TYPE_ENEMY);
    if (pEnemy) {
        HandleBulletEnemyCollision(pActor, pPrototype, pEnemy);
    }

    Game::GetAnimFrameFromDirection(pActor, pPrototype);
}

static void InitializeBullet(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pActor->state.bulletState.lifetimeCounter = pPrototype->data.bulletData.lifetime;
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
    {.name = "Death effect", .type = ACTOR_EDITOR_PROPERTY_ASSET, .assetType = ASSET_TYPE_ACTOR_PROTOTYPE, .offset = offsetof(BulletData, deathEffect) }
};

const ActorEditorData Editor::bulletEditorData = {
    { "Default", "Grenade" },
    { defaultProps, defaultProps },
};
#endif