#include "level_up.h"
#include "game_state.h"
#include "game_input.h"
#include "dialog.h"
#include "game_rendering.h"
#include "rendering.h"
#include "game.h"
#include <cstring>
#include <cstdio>

static Game::LevelUp::MenuState g_menuState = Game::LevelUp::MENU_CLOSED;
static Game::LevelUp::UpgradeOption g_selectedOption = Game::LevelUp::UPGRADE_HEALTH;

// Experience table for level progression
static constexpr s16 EXP_TABLE[] = {
    0,     // Level 1
    100,   // Level 2
    300,   // Level 3
    600,   // Level 4
    1000,  // Level 5
    1500,  // Level 6
    2100,  // Level 7
    2800,  // Level 8
    3600,  // Level 9
    4500,  // Level 10
    5500,  // Level 11
    6600,  // Level 12
    7800,  // Level 13
    9100,  // Level 14
    10500, // Level 15
    12000, // Level 16
    13600, // Level 17
    15300, // Level 18
    17100, // Level 19
    19000  // Level 20 (max)
};
static constexpr s16 MAX_LEVEL = 20;

static void DrawLevelUpMenu() {
    if (g_menuState != Game::LevelUp::MENU_OPEN) {
        return;
    }

    // Simple text display using the dialog system's text rendering approach
    // For now, we'll just ensure the dialog box is drawn and handle text separately
}

bool Game::LevelUp::OpenLevelUpMenu() {
    if (g_menuState != MENU_CLOSED || !CanLevelUp()) {
        return false;
    }

    // Open dialog box for the menu
    if (!Game::OpenDialog({4, 2}, {16, 12})) {
        return false;
    }

    g_menuState = MENU_OPENING;
    g_selectedOption = UPGRADE_HEALTH;
    
    return true;
}

void Game::LevelUp::CloseLevelUpMenu() {
    if (g_menuState == MENU_OPEN) {
        Game::CloseDialog();
        g_menuState = MENU_CLOSING;
    }
}

bool Game::LevelUp::UpdateLevelUpMenu() {
    // Update dialog state
    bool dialogActive = Game::UpdateDialog();
    
    // Sync menu state with dialog state
    if (!dialogActive) {
        if (g_menuState == MENU_CLOSING) {
            g_menuState = MENU_CLOSED;
        }
        return false;
    }
    
    if (g_menuState == MENU_OPENING && Game::IsDialogOpen()) {
        g_menuState = MENU_OPEN;
    }
    
    // Handle input when menu is open
    if (g_menuState == MENU_OPEN) {
        // Navigation
        if (Game::Input::ButtonPressed(BUTTON_DPAD_UP)) {
            g_selectedOption = (g_selectedOption == UPGRADE_HEALTH) ? UPGRADE_STAMINA : UPGRADE_HEALTH;
        }
        if (Game::Input::ButtonPressed(BUTTON_DPAD_DOWN)) {
            g_selectedOption = (g_selectedOption == UPGRADE_STAMINA) ? UPGRADE_HEALTH : UPGRADE_STAMINA;
        }
        
        // Confirm selection
        if (Game::Input::ButtonPressed(BUTTON_A)) {
            ApplyUpgrade(g_selectedOption);
            CloseLevelUpMenu();
        }
        
        // Cancel
        if (Game::Input::ButtonPressed(BUTTON_B)) {
            CloseLevelUpMenu();
        }
        
        DrawLevelUpMenu();
    }
    
    return g_menuState != MENU_CLOSED;
}

bool Game::LevelUp::IsLevelUpMenuActive() {
    return g_menuState != MENU_CLOSED;
}

s16 Game::LevelUp::GetExpRequiredForLevel(s16 level) {
    if (level <= 1) return 0;
    if (level > MAX_LEVEL) return EXP_TABLE[MAX_LEVEL - 1];
    return EXP_TABLE[level - 1];
}

s16 Game::LevelUp::CalculateLevelFromExp(s16 exp) {
    for (s16 level = 1; level <= MAX_LEVEL; level++) {
        if (exp < GetExpRequiredForLevel(level)) {
            return level - 1;
        }
    }
    return MAX_LEVEL;
}

bool Game::LevelUp::CanLevelUp() {
    s16 currentLevel = Game::GetPlayerLevel();
    s16 playerExp = Game::GetPlayerExp();
    s16 calculatedLevel = CalculateLevelFromExp(playerExp);
    
    return calculatedLevel > currentLevel && currentLevel < MAX_LEVEL;
}

void Game::LevelUp::ApplyUpgrade(UpgradeOption option) {
    // Level up the player
    s16 newLevel = Game::GetPlayerLevel() + 1;
    Game::SetPlayerLevel(newLevel);
    
    // Apply the chosen upgrade
    switch (option) {
    case UPGRADE_HEALTH: {
        s16 maxHealth = Game::GetPlayerMaxHealth();
        Game::SetPlayerMaxHealth(maxHealth + 5);
        
        // Also increase current health
        Game::SetPlayerHealth(maxHealth + 5);
        break;
    }
    case UPGRADE_STAMINA: {
        s16 maxStamina = Game::GetPlayerMaxStamina();
        Game::SetPlayerMaxStamina(maxStamina + 10);
        
        // Also increase current stamina
        Game::SetPlayerStamina(maxStamina + 10);
        break;
    }
    default:
        break;
    }
}