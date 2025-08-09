#ifdef HEADLESS

#include "input.h"
#include "typedef.h"

// Forward declaration to match the interface
union SDL_Event;

// Headless input - provides the same interface but allows programmatic input injection

static u16 controllerState = 0;

namespace Input {
    void ProcessEvent(const SDL_Event* event) {
        // No-op in headless mode since we don't have SDL events
    }

    u16 GetControllerState() {
        return controllerState;
    }
    
    // Headless-specific function to inject input for testing
    void SetControllerState(u16 state) {
        controllerState = state;
    }
}

#endif // HEADLESS