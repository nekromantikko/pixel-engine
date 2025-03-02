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
    BUTTON_DPAD_RIGHT = 1 << 7,

    BUTTON_DPAD_ALL = 0xF0,
    BUTTON_ALL = 0xFF
};

namespace Game {
	namespace Input {
        bool ButtonDown(u8 flags);
        bool ButtonUp(u8 flags);
        bool ButtonPressed(u8 flags);
        bool ButtonReleased(u8 flags);
        bool AnyButtonDown(u8 mask = BUTTON_ALL);
        bool AnyButtonUp(u8 mask = BUTTON_ALL);
        bool AnyButtonPressed(u8 mask = BUTTON_ALL);
        bool AnyButtonReleased(u8 mask = BUTTON_ALL);

        void Update();
	}
}