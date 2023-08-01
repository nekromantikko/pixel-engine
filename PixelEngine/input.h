#pragma once
#include "typedef.h"

namespace Input {
    enum ControllerState : u8 {
        None = 0,
        A = 1 << 0,
        B = 1 << 1,
        Select = 1 << 2,
        Start = 1 << 3,
        DPadUp = 1 << 4,
        DPadDown = 1 << 5,
        DPadLeft = 1 << 6,
        DPadRight = 1 << 7
    };

    void Poll();

    bool Down(ControllerState flags);
    bool Up(ControllerState flags);
    bool Pressed(ControllerState flags);
    bool Released(ControllerState flags);
}