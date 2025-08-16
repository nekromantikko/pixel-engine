#include "actors.h"
#include "game.h"
#include "game_rendering.h"
#include "audio.h"
#include <cstdio>
#include <cstring>

// Calls itoa, but adds a plus sign if value is positive
static size_t ItoaSigned(s16 value, char* str) {
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
    const u8 strLength = (u8)ItoaSigned(value, numberStr);

    // Ascii character '*' = 0x2A
    constexpr u8 chrOffset = 0x2A;
	constexpr u16 chrWidth = 6;
    const u8 palette = damage.flags.healing ? 0x3 : 0x1;

    const glm::i16vec2 pixelPos = Game::Rendering::WorldPosToScreenPixels(pActor->position);

    const s16 widthPx = strLength * chrWidth;
    const s16 xStart = pixelPos.x - widthPx / 2;

    bool result = true;
    for (u8 i = 0; i < strLength; i++) {
        Sprite sprite{};
        sprite.tileId = 0x90 + numberStr[i] - chrOffset;
        sprite.palette = palette;
        sprite.x = xStart + i * chrWidth;
        sprite.y = pixelPos.y;
        result &= Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }

	if (damage.flags.crit) {
        const s16 xCrit = pixelPos.x - chrWidth * 2;
		const s16 yCrit = pixelPos.y - TILE_DIM_PIXELS;
		for (u32 i = 0; i < 4; i++) {
			Sprite sprite{};
			sprite.tileId = 0xa0 + i;
			sprite.palette = palette;
			sprite.x = xCrit + i * chrWidth;
			sprite.y = yCrit;
			result &= Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
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

static void UpdateRain(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    // Fast vertical fall with slight wind drift
    constexpr r32 fallSpeed = 0.125f; // Much faster than feather
    constexpr r32 windStrength = 0.02f;
    constexpr r32 windFreq = 1 / 60.f;
    
    const u16 time = pActor->state.effectState.lifetimeCounter - pActor->state.effectState.initialLifetime;
    const r32 windOffset = glm::sin(time * windFreq) * windStrength;
    
    pActor->velocity.y = fallSpeed;
    pActor->velocity.x = pActor->initialVelocity.x + windOffset;
    pActor->position += pActor->velocity;
}

static void UpdateSnow(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    // Slow fall with wind drift and occasional spiraling  
    constexpr r32 fallSpeed = 0.02f; // Very slow fall
    constexpr r32 windStrength = 0.04f;
    constexpr r32 windFreq = 1 / 90.f;
    constexpr r32 spiralFreq = 1 / 120.f;
    constexpr r32 spiralRadius = 0.5f;
    
    const u16 time = pActor->state.effectState.lifetimeCounter - pActor->state.effectState.initialLifetime;
    const r32 windOffset = glm::sin(time * windFreq) * windStrength;
    const r32 spiralX = glm::cos(time * spiralFreq) * spiralRadius * 0.01f;
    
    pActor->velocity.y = fallSpeed;
    pActor->velocity.x = pActor->initialVelocity.x + windOffset + spiralX;
    pActor->position += pActor->velocity;
}

static void UpdateAsh(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    // Very slow rise/float with strong wind effects
    constexpr r32 riseSpeed = -0.01f; // Negative for upward movement
    constexpr r32 windStrength = 0.06f;
    constexpr r32 windFreq = 1 / 45.f;
    constexpr r32 turbulenceFreq = 1 / 30.f;
    constexpr r32 turbulenceStrength = 0.03f;
    
    const u16 time = pActor->state.effectState.lifetimeCounter - pActor->state.effectState.initialLifetime;
    const r32 windOffset = glm::sin(time * windFreq) * windStrength;
    const r32 turbulence = glm::sin(time * turbulenceFreq) * turbulenceStrength;
    
    pActor->velocity.y = riseSpeed + turbulence;
    pActor->velocity.x = pActor->initialVelocity.x + windOffset;
    pActor->position += pActor->velocity;
}

static void UpdateFireflies(Actor* pActor, const ActorPrototype* pPrototype) {
    if (!Game::UpdateCounter(pActor->state.effectState.lifetimeCounter)) {
        pActor->flags.pendingRemoval = true;
    }

    // Floating with sine wave patterns and blinking effect
    constexpr r32 floatSpeed = 0.005f;
    constexpr r32 horizontalFreq = 1 / 120.f;
    constexpr r32 verticalFreq = 1 / 80.f;
    constexpr r32 horizontalAmplitude = 0.03f;
    constexpr r32 verticalAmplitude = 0.02f;
    constexpr u16 blinkInterval = 60; // Blink every 60 frames (1 second at 60fps)
    
    const u16 time = pActor->state.effectState.lifetimeCounter - pActor->state.effectState.initialLifetime;
    const r32 horizontalSine = glm::sin(time * horizontalFreq) * horizontalAmplitude;
    const r32 verticalSine = glm::sin(time * verticalFreq) * verticalAmplitude;
    
    pActor->velocity.x = pActor->initialVelocity.x + horizontalSine;
    pActor->velocity.y = floatSpeed + verticalSine;
    
    // Blinking effect - toggle visibility based on time
    const bool shouldBlink = (time / blinkInterval) % 2 == 0;
    pActor->drawState.visible = shouldBlink || (time % blinkInterval) < (blinkInterval / 3);
    
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
    InitBaseEffect,
    InitBaseEffect, // Rain
    InitBaseEffect, // Snow
    InitBaseEffect, // Ash
    InitBaseEffect  // Fireflies
};
constexpr ActorUpdateFn Game::effectUpdateTable[EFFECT_TYPE_COUNT] = {
    UpdateDmgNumbers,
    UpdateExplosion,
    UpdateFeather,
    UpdateRain,
    UpdateSnow,
    UpdateAsh,
    UpdateFireflies,
};
constexpr ActorDrawFn Game::effectDrawTable[EFFECT_TYPE_COUNT] = {
    DrawDmgNumbers,
    Game::DrawActorDefault,
    Game::DrawActorDefault,
    Game::DrawActorDefault, // Rain
    Game::DrawActorDefault, // Snow
    Game::DrawActorDefault, // Ash
    Game::DrawActorDefault, // Fireflies
};