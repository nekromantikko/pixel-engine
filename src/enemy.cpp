#include "actors.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"

// TODO: Should be determined by enemy stats
constexpr u16 baseDamage = 10;

static void UpdateSlimeEnemy(Actor* pActor, const ActorPrototype* pPrototype) {
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
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        pActor->velocity.x = 0.0f;
        pActor->flags.facingDir = (s8)hit.impactNormal.x;
    }

    Game::ApplyGravity(pActor);

    // Reset in air flag
    pActor->flags.inAir = true;

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
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

static void UpdateSkullEnemy(Actor* pActor, const ActorPrototype* pPrototype) {
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

            Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
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

static void FireballDie(Actor* pActor, const ActorPrototype* pPrototype, const glm::vec2& effectPos) {
    pActor->flags.pendingRemoval = true;
    Game::SpawnActor(pPrototype->data.fireballData.deathEffect, effectPos);
}

static void UpdateFireball(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.fireballState.lifetimeCounter)) {
        return FireballDie(pActor, pPrototype, pActor->position);
    }

    HitResult hit{};
    if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
        return FireballDie(pActor, pPrototype, hit.impactPoint);

    }

    if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
        return FireballDie(pActor, pPrototype, hit.impactPoint);
    }

    Actor* pPlayer = Game::GetPlayer();
    const Damage damage = Game::CalculateDamage(pPlayer, baseDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
        return FireballDie(pActor, pPrototype, pActor->position);
    }

    Game::AdvanceCurrentAnimation(pActor, pPrototype);
}

static void UpdateAxolotlBoss(Actor* pActor, const ActorPrototype* pPrototype) {
    Game::UpdateCounter(pActor->state.enemyState.damageCounter);

    const u16 maxHealth = pPrototype->data.enemyData.health;
    const u16 currentHealth = pActor->state.enemyState.health;
    const r32 healthPercent = (r32)currentHealth / (r32)maxHealth;

    // Phase determination based on health
    const bool phase1 = healthPercent > 0.5f;  // 100% - 50% health
    const bool phase2 = healthPercent > 0.25f && !phase1;  // 50% - 25% health  
    const bool phase3 = healthPercent <= 0.25f;  // 25% - 0% health (rage mode)

    Game::ActorFacePlayer(pActor);

    // Movement behavior varies by phase
    if (phase1) {
        // Phase 1: Ground-based movement with charge attacks
        if (!pActor->flags.inAir) {
            const bool shouldCharge = Random::GenerateInt(0, 255) == 0;
            if (shouldCharge) {
                // Charge attack towards player
                pActor->velocity.y = -0.125f;  // Small hop
                pActor->velocity.x = 0.25f * pActor->flags.facingDir;  // Fast horizontal movement
            }
            else {
                // Regular movement
                pActor->velocity.x = 0.0125f * pActor->flags.facingDir;
            }
        }
    }
    else if (phase2) {
        // Phase 2: Swimming/floating behavior with projectile attacks
        static const r32 amplitude = 6.0f;
        const r32 sineTime = glm::sin(Game::GetFramesElapsed() / 45.f);
        pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

        // Horizontal swimming movement
        pActor->velocity.x = 0.025f * pActor->flags.facingDir;

        // Shoot water projectiles more frequently
        const bool shouldFire = Random::GenerateInt(0, 63) == 0;
        if (shouldFire) {
            Actor* pPlayer = Game::GetPlayer();
            if (pPlayer != nullptr) {
                const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
                const glm::vec2 velocity = playerDir * 0.125f;  // Faster projectiles
                Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
            }
        }
    }
    else if (phase3) {
        // Phase 3: Rage mode - fast erratic movement and multiple attacks
        static const r32 amplitude = 8.0f;
        const r32 fastSineTime = glm::sin(Game::GetFramesElapsed() / 30.f);  // Faster oscillation
        pActor->position.y = pActor->initialPosition.y + fastSineTime * amplitude;

        // Erratic movement pattern
        pActor->velocity.x = 0.0375f * pActor->flags.facingDir;

        // Rapid fire projectiles
        const bool shouldFire = Random::GenerateInt(0, 31) == 0;
        if (shouldFire) {
            Actor* pPlayer = Game::GetPlayer();
            if (pPlayer != nullptr) {
                // Fire multiple projectiles in different directions
                const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
                
                // Center projectile
                Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, playerDir * 0.15f);
                
                // Spread projectiles (if we have enough randomness)
                if (Random::GenerateInt(0, 1) == 0) {
                    const r32 spreadAngle = 0.5f;
                    const glm::vec2 leftSpread = glm::vec2(
                        playerDir.x * glm::cos(spreadAngle) - playerDir.y * glm::sin(spreadAngle),
                        playerDir.x * glm::sin(spreadAngle) + playerDir.y * glm::cos(spreadAngle)
                    );
                    const glm::vec2 rightSpread = glm::vec2(
                        playerDir.x * glm::cos(-spreadAngle) - playerDir.y * glm::sin(-spreadAngle),
                        playerDir.x * glm::sin(-spreadAngle) + playerDir.y * glm::cos(-spreadAngle)
                    );
                    
                    Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, leftSpread * 0.15f);
                    Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, rightSpread * 0.15f);
                }
            }
        }
    }

    // Handle collision movement (for phase 1 mainly)
    if (phase1) {
        HitResult hit{};
        if (Game::ActorMoveHorizontal(pActor, pPrototype, hit)) {
            pActor->velocity.x = 0.0f;
            pActor->flags.facingDir = (s8)hit.impactNormal.x;
        }

        Game::ApplyGravity(pActor);

        // Reset in air flag
        pActor->flags.inAir = true;

        if (Game::ActorMoveVertical(pActor, pPrototype, hit)) {
            pActor->velocity.y = 0.0f;
            if (hit.impactNormal.y < 0.0f) {
                pActor->flags.inAir = false;
            }
        }
    }

    // Damage player on contact
    Actor* pPlayer = Game::GetPlayer();
    const u16 bossDamage = phase3 ? baseDamage * 2 : (phase2 ? baseDamage * 1.5f : baseDamage);  // Increased damage in later phases
    const Damage damage = Game::CalculateDamage(pPlayer, bossDamage);
    if (pPlayer && !Game::PlayerInvulnerable(pPlayer) && Game::ActorsColliding(pActor, pPlayer)) {
        Game::PlayerTakeDamage(pPlayer, damage, pActor->position);
    }

    pActor->drawState.hFlip = pActor->flags.facingDir == ACTOR_FACING_LEFT;
    Game::SetDamagePaletteOverride(pActor, pActor->state.enemyState.damageCounter);
}

static void InitEnemy(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    pActor->state.enemyState.health = pPrototype->data.enemyData.health;
    pActor->state.enemyState.damageCounter = 0;
    pActor->drawState.layer = SPRITE_LAYER_FG;
}

#pragma region Public API
void Game::EnemyDie(Actor* pActor, const ActorPrototype* pPrototype) {
    pActor->flags.pendingRemoval = true;

    PersistedActorData* pPersistData = GetPersistedActorData(pActor->persistId);
    if (pPersistData) {
        pPersistData->dead = true;
    }
    else SetPersistedActorData(pActor->persistId, { .dead = true });

    SpawnActor(pPrototype->data.enemyData.deathEffect, pActor->position);

    // Spawn exp halos
    const u16 totalExpValue = pPrototype->data.enemyData.expValue;
    Actor* pExpSpawner = SpawnActor(pPrototype->data.enemyData.expSpawner, pActor->position);
    pExpSpawner->state.expSpawner.remainingValue = totalExpValue;

    // Spawn loot
    SpawnActor(pPrototype->data.enemyData.lootSpawner, pActor->position);
}
#pragma endregion

constexpr ActorInitFn Game::enemyInitTable[ENEMY_TYPE_COUNT] = {
    InitEnemy,
    InitEnemy,
    InitEnemy,
    InitEnemy,  // Axolotl boss uses the same init function
};
constexpr ActorUpdateFn Game::enemyUpdateTable[ENEMY_TYPE_COUNT] = {
    UpdateSlimeEnemy,
    UpdateSkullEnemy,
    UpdateFireball,
    UpdateAxolotlBoss,
};
constexpr ActorDrawFn Game::enemyDrawTable[ENEMY_TYPE_COUNT] = {
    Game::DrawActorDefault,
    Game::DrawActorDefault,
    Game::DrawActorDefault,
    Game::DrawActorDefault,  // Axolotl boss uses default drawing
};