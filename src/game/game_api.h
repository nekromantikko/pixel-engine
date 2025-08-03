#pragma once
/*
 * Game Module Public API
 * 
 * Provides the main game loop and core game functionality.
 * This is the primary interface for game logic.
 */

// Main game interface - this is what external code should use
#include "game.h"

// Note: All other game files (actors, player, enemies, etc.) are internal implementation details
// External code should interact with the game through the main Game namespace only