#pragma once
#include "typedef.h"

namespace Game {
    namespace LevelUp {
        enum MenuState {
            MENU_CLOSED = 0,
            MENU_OPENING,
            MENU_OPEN,
            MENU_CLOSING
        };

        enum UpgradeOption {
            UPGRADE_HEALTH = 0,
            UPGRADE_STAMINA,
            UPGRADE_COUNT
        };

        // Initialize level up menu
        bool OpenLevelUpMenu();
        
        // Close level up menu
        void CloseLevelUpMenu();
        
        // Update menu logic and input handling
        bool UpdateLevelUpMenu();
        
        // Check if menu is active
        bool IsLevelUpMenuActive();
        
        // Get experience required for next level
        s16 GetExpRequiredForLevel(s16 level);
        
        // Calculate current level from experience
        s16 CalculateLevelFromExp(s16 exp);
        
        // Check if player can level up
        bool CanLevelUp();
        
        // Apply upgrade choice
        void ApplyUpgrade(UpgradeOption option);
    }
}