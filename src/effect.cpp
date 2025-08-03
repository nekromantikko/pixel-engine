#include "actor_tables.h"
#include "actors.h"
#include "actor_prototype_types.h"
#include "game_rendering.h"
#include "audio.h"
#include <cstdio>
#include <cstring>

// Calls itoa, but adds a plus sign if value is positive
static u32 ItoaSigned(s16 value, char* str) {
    s32 i = 0;
    if (value > 0) {
        str[i++] = '+';
    }

    sprintf(str + i, "%d", value);

    return strlen(str);
}

static bool DrawDmgNumbers(const Actor* pActor, const ActorPrototype* pPrototype) {
    const Damage& damage = pActor->state.dmgNumberState.damage;

    static char numberStr[16]{};

    s16 value = damage.value;
    if (!damage.flags.healing) {
        value *= -1;
    }
    const u32 strLength = ItoaSigned(value, numberStr);

    // Ascii character '*' = 0x2A
    constexpr u8 chrOffset = 0x2A;
	constexpr u16 chrWidth = 6;
    const u8 palette = damage.flags.healing ? 0x3 : 0x1;

    const glm::i16vec2 pixelPos = Game::Rendering::WorldPosToScreenPixels(pActor->position);

    const s16 widthPx = strLength * chrWidth;
    const s16 xStart = pixelPos.x - widthPx / 2;

    bool result = true;
    for (u32 i = 0; i < strLength; i++) {
        Sprite sprite{};
        sprite.tileId = 0xe0 + numberStr[i] - chrOffset;
        sprite.palette = palette;
        sprite.x = xStart + i * chrWidth;
        sprite.y = pixelPos.y;
        result &= Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);
    }

	if (damage.flags.crit) {
        const s16 xCrit = pixelPos.x - chrWidth * 2;
		const s16 yCrit = pixelPos.y - TILE_DIM_PIXELS;
		for (u32 i = 0; i < 4; i++) {
			Sprite sprite{};
			sprite.tileId = 0xf0 + i;
			sprite.palette = palette;
			sprite.x = xCrit + i * chrWidth;
			sprite.y = yCrit;
			result &= Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);
		}
	}

    return result;
}

static void UpdateExplosion(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    Game::AdvanceCurrentAnimation(pActor, pPrototype);
}

static void UpdateDmgNumbers(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.dmgNumberState.base.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    pActor->position.y += pActor->velocity.y;
}

static void UpdateFeather(Actor* pActor, const ActorPrototype* pPrototype) {
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

static void InitEffectState(EffectState& state, const EffectData& data) {
    state.initialLifetime = data.lifetime;
    state.lifetimeCounter = data.lifetime;
    if (data.sound != SoundHandle::Null()) {
        Audio::PlaySFX(data.sound);
    }
}

static void InitDmgNumbers(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
    return InitEffectState(pActor->state.dmgNumberState.base, pPrototype->data.effectData);
}

static void InitBaseEffect(Actor* pActor, const ActorPrototype* pPrototype, const PersistedActorData* pPersistData) {
	pActor->drawState.layer = SPRITE_LAYER_FX;
    return InitEffectState(pActor->state.effectState, pPrototype->data.effectData);
}

constexpr ActorInitFn Game::effectInitTable[EFFECT_TYPE_COUNT] = {
    InitDmgNumbers,
    InitBaseEffect,
    InitBaseEffect
};
constexpr ActorUpdateFn Game::effectUpdateTable[EFFECT_TYPE_COUNT] = {
    UpdateDmgNumbers,
    UpdateExplosion,
    UpdateFeather,
};
constexpr ActorDrawFn Game::effectDrawTable[EFFECT_TYPE_COUNT] = {
    DrawDmgNumbers,
    Game::DrawActorDefault,
    Game::DrawActorDefault,
};