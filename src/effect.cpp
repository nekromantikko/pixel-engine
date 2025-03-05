#include "effect.h"
#include "actors.h"
#include "actor_prototypes.h"
#include "game_rendering.h"

// Calls itoa, but adds a plus sign if value is positive
static u32 ItoaSigned(s16 value, char* str) {
    s32 i = 0;
    if (value > 0) {
        str[i++] = '+';
    }

    itoa(value, str + i, 10);

    return strlen(str);
}

static bool DrawNumbers(const Actor* pActor) {
    const Animation& currentAnim = pActor->pPrototype->animations[pActor->drawState.animIndex];
    if (currentAnim.type != ANIMATION_TYPE_SPRITES) {
        return false;
    }

    static char numberStr[16]{};
    const u32 strLength = ItoaSigned(pActor->state.dmgNumberState.value, numberStr);

    // Ascii character '*' = 0x2A
    constexpr u8 chrOffset = 0x2A;

    const s32 frameCount = currentAnim.frameCount;
    const glm::i16vec2 pixelPos = Game::Rendering::WorldPosToScreenPixels(pActor->position);

    bool result = true;
    for (u32 c = 0; c < strLength; c++) {
        glm::i16vec2 drawPos = pixelPos + glm::i16vec2{ c * 5, 0 };
        const s32 frameIndex = (numberStr[c] - chrOffset) % frameCount;

        result &= Game::Rendering::DrawMetaspriteSprite(pActor->drawState.layer, currentAnim.metaspriteIndex, frameIndex, drawPos, false, false, -1);
    }

    return result;
}

static void UpdateExplosion(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    Game::AdvanceCurrentAnimation(pActor);
}

static void UpdateNumbers(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.dmgNumberState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    pActor->position.y += pActor->velocity.y;
}

static void UpdateFeather(Actor* pActor) {
    if (!Game::UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    constexpr r32 maxFallSpeed = 0.03125f;
    Game::ApplyGravity(pActor, 0.005f);
    if (pActor->velocity.y > maxFallSpeed) {
        pActor->velocity.y = maxFallSpeed;
    }

    constexpr r32 amplitude = 2.0f;
    constexpr r32 timeMultiplier = 1 / 30.f;
    const u16 time = pActor->state.effectState.lifetimeCounter - pActor->state.effectState.initialLifetime;
    const r32 sineTime = glm::sin(time * timeMultiplier);
    pActor->velocity.x = pActor->initialVelocity.x * sineTime;

    pActor->position += pActor->velocity;
}

static void InitializeEffect(Actor* pActor, const PersistedActorData* pPersistData) {
	pActor->state.effectState.initialLifetime = pActor->pPrototype->data.effectData.lifetime;
	pActor->state.effectState.lifetimeCounter = pActor->pPrototype->data.effectData.lifetime;
	pActor->drawState.layer = SPRITE_LAYER_FX;
}

constexpr ActorInitFn Game::effectInitTable[EFFECT_TYPE_COUNT] = {
    InitializeEffect,
    InitializeEffect,
    InitializeEffect
};
constexpr ActorUpdateFn Game::effectUpdateTable[EFFECT_TYPE_COUNT] = {
    UpdateNumbers,
    UpdateExplosion,
    UpdateFeather,
};
constexpr ActorDrawFn Game::effectDrawTable[EFFECT_TYPE_COUNT] = {
    DrawNumbers,
    Game::DrawActorDefault,
    Game::DrawActorDefault,
};

#ifdef EDITOR
static const std::initializer_list<ActorEditorProperty> defaultProps = {
    {.name = "Lifetime", .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_U16, .components = 1, .offset = offsetof(EffectData, lifetime) },
};

const ActorEditorData Editor::effectEditorData = {
    { "Damage numbers", "Explosion", "Feather" },
    { defaultProps, defaultProps, defaultProps }
};
#endif