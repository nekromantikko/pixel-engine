#include "game_ui.h"
#include "game_rendering.h"
#include "coroutines.h"

static HealthBarState playerHealthBarState{};
static ExpCounterState playerExpCounterState{};

static CoroutineHandle playerHealthCoroutine = CoroutineHandle::Null();
static CoroutineHandle playerExpCoroutine = CoroutineHandle::Null();

#pragma region Animation
struct HealthBarAnimState {
	const HealthBarState initialState;
    const HealthBarState targetState;
    HealthBarState& currentState;
    u16 delay;
    u16 duration;
    u16 progress;
};

static bool AnimateHealthCoroutine(void* userData) {
    HealthBarAnimState& state = *(HealthBarAnimState*)userData;

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

void Game::UI::DrawPlayerHealthBar(u16 maxHealth) {
    const u16 xStart = 16;
    const u16 y = 16;

    const u32 totalSegments = maxHealth >> 2;
    const u32 fullRedSegments = playerHealthBarState.red >> 2;
    const u32 fullYellowSegments = playerHealthBarState.yellow >> 2;

    u16 x = xStart;
    for (u32 i = 0; i < totalSegments; i++) {
        const u16 healthDrawn = (i * 4);
        const u16 remainingRedHealth = healthDrawn > playerHealthBarState.red ? 0 : playerHealthBarState.red - healthDrawn;
        const u16 remainingYellowHealth = healthDrawn > playerHealthBarState.yellow ? 0 : playerHealthBarState.yellow - healthDrawn;
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
        sprite.palette = 0x1;
        sprite.x = x;
        sprite.y = y;
        Rendering::DrawSprite(SPRITE_LAYER_UI, sprite);

        x += 8;
    }
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
    StopCoroutine(playerHealthCoroutine);

    // Set yellow health to previous red health
    playerHealthBarState.yellow = playerHealthBarState.red;

    // If taking damage, immediately set red health to new value
    if (health < playerHealthBarState.red) {
        playerHealthBarState.red = health;
    }

    HealthBarState targetState = {
        .red = health,
        .yellow = health
    };

    HealthBarAnimState state = {
        .initialState = playerHealthBarState,
        .targetState = targetState,
        .currentState = playerHealthBarState,
        .delay = 16,
        .duration = 20,
        .progress = 0
    };
    playerHealthCoroutine = Game::StartCoroutine(AnimateHealthCoroutine, state);
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