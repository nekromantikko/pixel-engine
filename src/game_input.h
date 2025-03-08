#pragma once
#include "typedef.h"

enum ControllerButtonFlags : u16 {
    BUTTON_NONE = 0,
    BUTTON_R = 1 << 4,
    BUTTON_L = 1 << 5,
    BUTTON_X = 1 << 6,
    BUTTON_A = 1 << 7,
    BUTTON_DPAD_RIGHT = 1 << 8,
    BUTTON_DPAD_LEFT = 1 << 9,
    BUTTON_DPAD_DOWN = 1 << 10,
    BUTTON_DPAD_UP = 1 << 11,
    BUTTON_START = 1 << 12,
    BUTTON_SELECT = 1 << 13,
    BUTTON_Y = 1 << 14,
    BUTTON_B = 1 << 15,


    BUTTON_DPAD_ALL = BUTTON_DPAD_UP | BUTTON_DPAD_DOWN | BUTTON_DPAD_LEFT | BUTTON_DPAD_RIGHT,
    BUTTON_ALL = BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y | BUTTON_L | BUTTON_R | BUTTON_START | BUTTON_SELECT | BUTTON_DPAD_ALL,
};

namespace Game {
	namespace Input {
        bool ButtonDown(u16 flags);
        bool ButtonUp(u16 flags);
        bool ButtonPressed(u16 flags);
        bool ButtonReleased(u16 flags);
        bool AnyButtonDown(u16 mask = BUTTON_ALL);
        bool AnyButtonUp(u16 mask = BUTTON_ALL);
        bool AnyButtonPressed(u16 mask = BUTTON_ALL);
        bool AnyButtonReleased(u16 mask = BUTTON_ALL);

        void Update();
	}
}