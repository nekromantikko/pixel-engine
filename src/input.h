#pragma once
#include "typedef.h"

union SDL_Event;

namespace Input {
    void ProcessEvent(const SDL_Event* event);

    u16 GetControllerState();

#ifdef HEADLESS
    // Headless-specific functions for input injection during testing
    void SetControllerState(u16 state);
#endif
}