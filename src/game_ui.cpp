#include "game_ui.h"
#include "game_rendering.h"
#include "coroutines.h"

static BarState playerHealthBarState{};
static BarState playerStaminaBarState{};
static ExpCounterState playerExpCounterState{};

static CoroutineHandle playerHealthCoroutine = CoroutineHandle::Null();
static CoroutineHandle playerStaminaCoroutine = CoroutineHandle::Null();
static CoroutineHandle playerExpCoroutine = CoroutineHandle::Null();

#pragma region Animation
struct BarAnimState {
	const BarState initialState;
    const BarState targetState;
    BarState& currentState;
    u16 delay;
    u16 duration;
    u16 progress;
};

static bool AnimateBarCoroutine(void* userData) {
    BarAnimState& state = *(BarAnimState*)userData;

    if (state.delay > 0) {
        state.delay--;
        return true;
    }

    state.progress++;
    const r32 t = glm::smoothstep(0.0f, 1.0f, r32(state.progress) / state.duration);
    state.currentState.red = glm::mix(state.initialState.red, state.targetState.red, t);
    state.currentState.yellow = glm::mix(state.initialState.yellow, state.targetState.yellow, t);

	return state.progress < state.duration;
}

struct ExpCounterAnimState {
    const u16 initialExp;
	const u16 targetExp;

    u16 duration;
    u16 progress;
};

static bool AnimateExpCoroutine(void* userData) {
    ExpCounterAnimState& state = *(ExpCounterAnimState*)userData;

    state.progress++;
    const r32 t = glm::smoothstep(0.0f, 1.0f, r32(state.progress) / state.duration);
    playerExpCounterState.exp = glm::mix(state.initialExp, state.targetExp, t);

    return state.progress < state.duration;
}
#pragma endregion

static void AnimateBar(BarState& bar, u16 targetValue, CoroutineHandle& coroutineHandle) {
    Game::StopCoroutine(coroutineHandle);

    // Set yellow health to previous red health
    bar.yellow = bar.red;

    // If taking damage, immediately set red health to new value
    if (targetValue < bar.red) {
        bar.red = targetValue;
    }

    BarState targetState = {
        .red = targetValue,
        .yellow = targetValue
    };

    BarAnimState state = {
        .initialState = bar,
        .targetState = targetState,
        .currentState = bar,
        .delay = 16,
        .duration = 20,
        .progress = 0
    };
    coroutineHandle = Game::StartCoroutine(AnimateBarCoroutine, state);
}

static void DrawBar(const glm::i16vec2 pixelPos, const BarState& bar, u16 maxValue, u8 palette) {
    const u16 xStart = pixelPos.x;
    const u16 y = pixelPos.y;

    const u32 totalSegments = maxValue >> 2;
    const u32 fullRedSegments = bar.red >> 2;
    const u32 fullYellowSegments = bar.yellow >> 2;

    u16 x = xStart;
    for (u32 i = 0; i < totalSegments; i++) {
        const u16 healthDrawn = (i * 4);
        const u16 remainingRedHealth = healthDrawn > bar.red ? 0 : bar.red - healthDrawn;
        const u16 remainingYellowHealth = healthDrawn > bar.yellow ? 0 : bar.yellow - healthDrawn;
        const u16 redRemainder = remainingRedHealth & 3;
        const u16 yellowRemainder = remainingYellowHealth & 3;

        u8 tileId;
        if (i < fullRedSegments) {
            tileId = 0xd4;
        }
        else if (i < fullYellowSegments) {
            tileId = 0xd5 + redRemainder;
        }
        else {
            // Only red left to draw
            if (redRemainder >= yellowRemainder) {
                tileId = 0xd0 + redRemainder;
            }
            else {
                const u8 offset = redRemainder != 0 ? redRemainder + 1 : 0;
                tileId = 0xd8 + yellowRemainder + offset;
            }
        }

        Sprite sprite{};
        sprite.tileId = tileId;
        sprite.palette = palette;
        sprite.x = x;
        sprite.y = y;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

        x += 8;
    }
}

void Game::UI::DrawPlayerHealthBar(u16 maxHealth) {
    DrawBar({ 16, 16 }, playerHealthBarState, maxHealth, 0x1);
}

void Game::UI::DrawPlayerStaminaBar(u16 maxStamina) {
    DrawBar({ 16, 24 }, playerStaminaBarState, maxStamina, 0x3);
}

void Game::UI::DrawExpCounter() {
    static char buffer[10];
    snprintf(buffer, sizeof(buffer), "%05u", playerExpCounterState.exp);
    u32 length = strlen(buffer);

    const u16 xStart = VIEWPORT_WIDTH_PIXELS - 16 - (length * 8);
    const u16 y = 16;

    // Draw halo indicator
    Sprite sprite{};
    sprite.tileId = 0x68;
    sprite.palette = 0x0;
    sprite.x = xStart - 8;
    sprite.y = y;
    Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

    // Draw counter
    u16 x = xStart;
    for (u32 i = 0; i < length; i++) {
        Sprite sprite{};
        sprite.tileId = 0xc6 + buffer[i] - '0';
        sprite.palette = 0x1;
        sprite.x = x;
        sprite.y = y;
        Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

        x += 8;
    }
}

void Game::UI::SetPlayerDisplayHealth(u16 health) {
    AnimateBar(playerHealthBarState, health, playerHealthCoroutine);
}
void Game::UI::SetPlayerDisplayStamina(u16 stamina) {
    AnimateBar(playerStaminaBarState, stamina, playerStaminaCoroutine);
}
void Game::UI::SetPlayerDisplayExp(u16 exp) {
    StopCoroutine(playerExpCoroutine);

    ExpCounterAnimState state = {
            .initialExp = playerExpCounterState.exp,
            .targetExp = exp,
            .duration = 20,
            .progress = 0
    };
    playerExpCoroutine = Game::StartCoroutine(AnimateExpCoroutine, state);
}