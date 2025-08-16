#include "game.h"
#include "game_ui.h"
#include "game_rendering.h"
#include <cstdio>
#include <cstring>
#define GLM_FORCE_RADIANS
#include <glm.hpp>

constexpr u16 animDuration = 20;
constexpr u16 barAnimDelay = 16;

struct BarState {
    s16 red;
    s16 yellow;

    s16 targetValue;

    u16 delay;
};

struct ExpCounterState {
    s16 exp;

    s16 targetValue;
};

static BarState playerHealthBarState{};
static BarState playerStaminaBarState{};
static ExpCounterState playerExpCounterState{};

#pragma region Animation
static void UpdateBar(BarState& bar) {
    if (bar.delay > 0) {
        bar.delay--;
        return;
    }

    if (bar.targetValue > bar.red) {
        bar.red++;
    }
    else if (bar.targetValue < bar.red) {
        bar.red--;
    }

    if (bar.targetValue > bar.yellow) {
        bar.yellow++;
    }
    else if (bar.targetValue < bar.yellow) {
        bar.yellow--;
    }
}

static void AnimateBar(BarState& bar, u16 targetValue) {
    bar.targetValue = targetValue;

    if (bar.yellow < bar.red) {
        bar.yellow = bar.red;
    }

    // If taking damage, immediately set red health to new value
    if (targetValue < bar.red) {
        bar.red = targetValue;
        bar.delay = barAnimDelay;
    }
    else {
        bar.delay = 0;
    }
}

static void UpdateExpCounter() {
    const s16 difference = playerExpCounterState.targetValue - playerExpCounterState.exp;
    const s16 step = glm::max(1, glm::abs(difference) / 10); // Increase step based on difference

    if (difference > 0) {
        playerExpCounterState.exp += step;
        if (playerExpCounterState.exp > playerExpCounterState.targetValue) {
            playerExpCounterState.exp = playerExpCounterState.targetValue; // Clamp to target
        }
    }
    else if (difference < 0) {
        playerExpCounterState.exp -= step;
        if (playerExpCounterState.exp < playerExpCounterState.targetValue) {
            playerExpCounterState.exp = playerExpCounterState.targetValue; // Clamp to target
        }
    }
}
#pragma endregion

static void DrawBar(const glm::i16vec2 pixelPos, const BarState& bar, s16 maxValue, u8 palette) {
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
            tileId = 0x84;
        }
        else if (i < fullYellowSegments) {
            tileId = 0x85 + redRemainder;
        }
        else {
            // Only red left to draw
            if (redRemainder >= yellowRemainder) {
                tileId = 0x80 + redRemainder;
            }
            else {
                const u8 offset = redRemainder != 0 ? redRemainder + 1 : 0;
                tileId = 0x88 + yellowRemainder + offset;
            }
        }

        Sprite sprite{};
        sprite.tileId = tileId;
        sprite.palette = palette;
        sprite.x = x;
        sprite.y = y;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);

        x += 8;
    }
}

void Game::UI::DrawPlayerHealthBar(s16 maxHealth) {
    DrawBar({ 16, 16 }, playerHealthBarState, maxHealth, 0x1);
}

void Game::UI::DrawPlayerStaminaBar(s16 maxStamina) {
    DrawBar({ 16, 24 }, playerStaminaBarState, maxStamina, 0x3);
}

void Game::UI::DrawExpCounter() {
    static char buffer[10];
    snprintf(buffer, sizeof(buffer), "%05u", playerExpCounterState.exp);
    u8 length = (u8)strlen(buffer);

    const u16 xStart = VIEWPORT_WIDTH_PIXELS - 16 - (length * 8);
    const u16 y = 16;

    // Draw halo indicator
    {
        Sprite sprite{};
        sprite.tileId = 0x08;
        sprite.palette = 0x0;
        sprite.x = xStart - 8;
        sprite.y = y;
        Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }

    // Draw counter
    u16 x = xStart;
    for (u8 i = 0; i < length; i++) {
        Sprite sprite{};
        sprite.tileId = buffer[i];
        sprite.palette = 0x1;
        sprite.x = x;
        sprite.y = y;
        Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);

        x += 8;
    }
}

void Game::UI::SetPlayerDisplayHealth(s16 health) {
    AnimateBar(playerHealthBarState, health);
}
void Game::UI::SetPlayerDisplayStamina(s16 stamina) {
    AnimateBar(playerStaminaBarState, stamina);
}
void Game::UI::SetPlayerDisplayExp(s16 exp) {
    playerExpCounterState.targetValue = exp;
}

void Game::UI::Update() {
    UpdateBar(playerHealthBarState);
    UpdateBar(playerStaminaBarState);
    UpdateExpCounter();
}

void Game::UI::DrawText(const char* text, const glm::ivec2& position, u8 palette) {
    if (!text) return;
    
    u32 length = strlen(text);
    s32 x = position.x;
    s32 y = position.y;
    
    for (u32 i = 0; i < length; i++) {
        char c = text[i];
        if (c == '\0') break;
        
        // Convert ASCII to tile ID - assuming font starts at tile 0x20 (space)
        u8 tileId = c;
        
        Sprite sprite{};
        sprite.tileId = tileId;
        sprite.palette = palette;
        sprite.x = x;
        sprite.y = y;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
        
        x += 8; // Move to next character position
    }
}

void Game::UI::DrawMenuItem(const char* text, const glm::ivec2& position, bool selected, u8 palette) {
    // Draw selection indicator if selected
    if (selected) {
        Sprite sprite{};
        sprite.tileId = 0x3e; // '>' character as selection indicator
        sprite.palette = palette;
        sprite.x = position.x - 16;
        sprite.y = position.y;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }
    
    DrawText(text, position, palette);
}

void Game::UI::DrawSlider(const char* label, r32 value, const glm::ivec2& position, bool selected, u8 palette) {
    // Draw label
    DrawText(label, position, palette);
    
    // Calculate slider position
    glm::ivec2 sliderPos = position;
    sliderPos.x += strlen(label) * 8 + 16; // Add some spacing after label
    
    // Draw slider track (using dash characters)
    constexpr s32 sliderWidth = 10; // Number of segments
    for (s32 i = 0; i < sliderWidth; i++) {
        Sprite sprite{};
        sprite.tileId = 0x2d; // '-' character for track
        sprite.palette = palette;
        sprite.x = sliderPos.x + i * 8;
        sprite.y = sliderPos.y;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }
    
    // Draw slider handle
    s32 handlePos = s32(value * (sliderWidth - 1));
    Sprite handleSprite{};
    handleSprite.tileId = 0x7c; // '|' character for handle
    handleSprite.palette = palette;
    handleSprite.x = sliderPos.x + handlePos * 8;
    handleSprite.y = sliderPos.y;
    Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, handleSprite);
    
    // Draw selection indicator if selected
    if (selected) {
        Sprite sprite{};
        sprite.tileId = 0x3e; // '>' character as selection indicator
        sprite.palette = palette;
        sprite.x = position.x - 16;
        sprite.y = position.y;
        Game::Rendering::DrawSprite(SPRITE_LAYER_UI, Game::GetConfig().uiBankHandle, sprite);
    }
}