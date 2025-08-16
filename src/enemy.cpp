#include "actors.h"
#include "game_rendering.h"
#include "game_state.h"
#include "random.h"
#include <gtc/constants.hpp>

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

// Bullet hell pattern functions
static void SkullFireRadialBurst(Actor* pActor, const ActorPrototype* pPrototype, s32 projectileCount) {
    constexpr r32 baseSpeed = 0.0625f;
    const r32 angleStep = glm::two_pi<r32>() / projectileCount;
    
    for (s32 i = 0; i < projectileCount; i++) {
        const r32 angle = angleStep * i;
        const glm::vec2 velocity = glm::vec2(glm::cos(angle), glm::sin(angle)) * baseSpeed;
        Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
    }
}

static void SkullFireSpiral(Actor* pActor, const ActorPrototype* pPrototype, r32 spiralOffset) {
    constexpr r32 baseSpeed = 0.0625f;
    constexpr s32 projectileCount = 6;
    const r32 angleStep = glm::two_pi<r32>() / projectileCount;
    
    for (s32 i = 0; i < projectileCount; i++) {
        const r32 angle = angleStep * i + spiralOffset;
        const glm::vec2 velocity = glm::vec2(glm::cos(angle), glm::sin(angle)) * baseSpeed;
        Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
    }
}

static void SkullFireAimedSpread(Actor* pActor, const ActorPrototype* pPrototype, const glm::vec2& playerDir) {
    constexpr r32 baseSpeed = 0.0625f;
    constexpr s32 spreadCount = 5;
    constexpr r32 spreadAngle = glm::quarter_pi<r32>() / 2.0f; // 22.5 degrees total spread
    
    const r32 baseAngle = glm::atan(playerDir.y, playerDir.x);
    const r32 angleStep = spreadAngle / (spreadCount - 1);
    const r32 startAngle = baseAngle - spreadAngle * 0.5f;
    
    for (s32 i = 0; i < spreadCount; i++) {
        const r32 angle = startAngle + angleStep * i;
        const glm::vec2 velocity = glm::vec2(glm::cos(angle), glm::sin(angle)) * baseSpeed;
        Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
    }
}

static void SkullFireWaveSweep(Actor* pActor, const ActorPrototype* pPrototype, r32 sweepAngle) {
    constexpr r32 baseSpeed = 0.0625f;
    constexpr s32 projectileCount = 7;
    constexpr r32 waveSpread = glm::half_pi<r32>(); // 90 degrees
    const r32 angleStep = waveSpread / (projectileCount - 1);
    const r32 startAngle = sweepAngle - waveSpread * 0.5f;
    
    for (s32 i = 0; i < projectileCount; i++) {
        const r32 angle = startAngle + angleStep * i;
        const glm::vec2 velocity = glm::vec2(glm::cos(angle), glm::sin(angle)) * baseSpeed;
        Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
    }
}

static void UpdateSkullEnemy(Actor* pActor, const ActorPrototype* pPrototype) {
    Game::UpdateCounter(pActor->state.enemyState.damageCounter);

    Game::ActorFacePlayer(pActor);

    static const r32 amplitude = 4.0f;
    const r32 sineTime = glm::sin(Game::GetFramesElapsed() / 60.f);
    pActor->position.y = pActor->initialPosition.y + sineTime * amplitude;

    // Bullet hell patterns
    const u32 frameCount = Game::GetFramesElapsed();
    Actor* pPlayer = Game::GetPlayer();
    
    if (pPlayer != nullptr) {
        const glm::vec2 playerDir = glm::normalize(pPlayer->position - pActor->position);
        
        // Pattern cycle: each pattern lasts ~2 seconds (120 frames), with 0.5s gaps (30 frames)
        const u32 patternCycle = frameCount / 150; // 2.5 second cycles
        const u32 patternPhase = frameCount % 150;
        
        if (patternPhase < 120) { // Active shooting phase
            const u32 pattern = patternCycle % 4;
            
            switch (pattern) {
                case 0: // Single aimed shots (original behavior, but faster)
                    if (patternPhase % 20 == 0) { // Every 1/3 second
                        const glm::vec2 velocity = playerDir * 0.0625f;
                        Game::SpawnActor(pPrototype->data.enemyData.projectile, pActor->position, velocity);
                    }
                    break;
                    
                case 1: // Radial bursts
                    if (patternPhase % 30 == 0) { // Every 0.5 seconds
                        SkullFireRadialBurst(pActor, pPrototype, 8);
                    }
                    break;
                    
                case 2: // Spiral pattern
                    if (patternPhase % 10 == 0) { // Every 1/6 second
                        const r32 spiralOffset = (patternPhase / 10) * 0.2f;
                        SkullFireSpiral(pActor, pPrototype, spiralOffset);
                    }
                    break;
                    
                case 3: // Aimed spread with sweeping waves
                    if (patternPhase % 25 == 0) { // Every ~0.4 seconds
                        if ((patternPhase / 25) % 2 == 0) {
                            SkullFireAimedSpread(pActor, pPrototype, playerDir);
                        } else {
                            const r32 sweepAngle = glm::sin(patternPhase * 0.1f) * glm::pi<r32>();
                            SkullFireWaveSweep(pActor, pPrototype, sweepAngle);
                        }
                    }
                    break;
            }
        }
        // patternPhase >= 120: Rest phase (no shooting)
    }

    // Player collision damage
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