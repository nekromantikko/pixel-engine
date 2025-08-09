// Example of using headless mode for automated testing
// This is a simple demonstration of how you could extend the engine
// to support automated input testing in headless mode

#ifdef HEADLESS
#include "input.h"
#include "game_input.h"

void example_automated_test() {
    // Example: simulate pressing the A button
    Input::SetControllerState(BUTTON_A);
    
    // Run game logic for a few frames
    Game::Update(1.0/60.0); // Simulate one frame at 60 FPS
    
    // Example: simulate D-pad movement
    Input::SetControllerState(BUTTON_DPAD_RIGHT);
    Game::Update(1.0/60.0);
    
    // Clear input
    Input::SetControllerState(BUTTON_NONE);
    Game::Update(1.0/60.0);
    
    // This kind of pattern could be used to:
    // - Test player movement
    // - Test menu navigation  
    // - Test game state transitions
    // - Verify collision detection
    // - Test AI behavior
}

#endif // HEADLESS