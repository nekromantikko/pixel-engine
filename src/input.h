#pragma once
#include "typedef.h"

enum ControllerButtonFlags : u8 {
    BUTTON_NONE = 0,
    BUTTON_A = 1 << 0,
    BUTTON_B = 1 << 1,
    BUTTON_SELECT = 1 << 2,
    BUTTON_START = 1 << 3,
    BUTTON_DPAD_UP = 1 << 4,
    BUTTON_DPAD_DOWN = 1 << 5,
    BUTTON_DPAD_LEFT = 1 << 6,
    BUTTON_DPAD_RIGHT = 1 << 7
};

union SDL_Event;

namespace Input {
    void ProcessEvent(const SDL_Event* event);

    u8 GetControllerState();

    // Utis
    bool ButtonDown(u8 flags, u8 state);
    bool ButtonUp(u8 flags, u8 state);
    bool ButtonPressed(u8 flags, u8 currentState, u8 previousState);
    bool ButtonReleased(u8 flags, u8 currentState, u8 previousState);
}