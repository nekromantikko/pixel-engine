#pragma once
#include "typedef.h"
#include <SDL.h>

namespace Input {
    enum ControllerState : u8 {
        CSTATE_NONE = 0,
        CSTATE_A = 1 << 0,
        CSTATE_B = 1 << 1,
        CSTATE_SELECT = 1 << 2,
        CSTATE_START = 1 << 3,
        CSTATE_DPAD_UP = 1 << 4,
        CSTATE_DPAD_DOWN = 1 << 5,
        CSTATE_DPAD_LEFT = 1 << 6,
        CSTATE_DPAD_RIGHT = 1 << 7
    };

    void Update();
    void ProcessEvent(const SDL_Event& event);

    bool Down(u8 flags);
    bool Up(u8 flags);
    bool Pressed(u8 flags);
    bool Released(u8 flags);
}