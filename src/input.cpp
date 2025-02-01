#include "input.h"
#include "system.h"
#include <SDL.h>
#include <cmath>

namespace Input {
    //TODO: multiple controller support
    SDL_GameController* gameController = nullptr;
    SDL_Haptic* haptic = nullptr;

    u8 currentState = CSTATE_NONE;
    u8 previousState = CSTATE_NONE;

    static void InitController() {
        if (SDL_NumJoysticks()) {
            gameController = SDL_GameControllerOpen(0);
            haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(gameController));
            SDL_HapticRumbleInit(haptic);
        }
        else gameController = nullptr;
    }

    static void FreeController() {
        if (gameController) {
            SDL_GameControllerClose(gameController);
        }
        if (haptic) {
            SDL_HapticRumbleStop(haptic);
            SDL_HapticClose(haptic);
        }
    }

    static void HandleControllerButtonEvent(const SDL_ControllerButtonEvent& event, u8& outState)
    {
        const bool pressed = (event.state == SDL_PRESSED);

        switch (event.button)
        {
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            pressed ? outState |= CSTATE_DPAD_LEFT : outState &= ~CSTATE_DPAD_LEFT;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            pressed ? outState |= CSTATE_DPAD_RIGHT : outState &= ~CSTATE_DPAD_RIGHT;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            pressed ? outState |= CSTATE_DPAD_UP : outState &= ~CSTATE_DPAD_UP;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            pressed ? outState |= CSTATE_DPAD_DOWN : outState &= ~CSTATE_DPAD_DOWN;
            break;
        case SDL_CONTROLLER_BUTTON_A:
            pressed ? outState |= CSTATE_A : outState &= ~CSTATE_A;
            break;
        case SDL_CONTROLLER_BUTTON_B:
            pressed ? outState |= CSTATE_B : outState &= ~CSTATE_B;
            break;
        case SDL_CONTROLLER_BUTTON_BACK:
            pressed ? outState |= CSTATE_SELECT : outState &= ~CSTATE_SELECT;
            break;
        case SDL_CONTROLLER_BUTTON_START:
            pressed ? outState |= CSTATE_START : outState &= ~CSTATE_START;
            break;
        default:
            break;
        }
    }

    static void HandleControllerAxisEvent(const SDL_ControllerAxisEvent& event, u8& outState)
    {
        // TODO
    }

    static void HandleControllerDeviceEvent(const SDL_ControllerDeviceEvent& event)
    {
        switch (event.type)
        {
        case SDL_CONTROLLERDEVICEADDED:
            if (event.which == 0)
            {
                if (!gameController)
                    InitController();
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (event.which == 0)
            {
                if (gameController)
                    FreeController();
            }

            break;
        default:
            break;
        }
    }

    static void HandleKeyboardEvent(const SDL_KeyboardEvent& event, u8& outState) {
        const bool pressed = (event.state == SDL_PRESSED);

        switch (event.keysym.scancode)
        {
        case SDL_SCANCODE_LEFT:
            pressed ? outState |= CSTATE_DPAD_LEFT : outState &= ~CSTATE_DPAD_LEFT;
            break;
        case SDL_SCANCODE_RIGHT:
            pressed ? outState |= CSTATE_DPAD_RIGHT : outState &= ~CSTATE_DPAD_RIGHT;
            break;
        case SDL_SCANCODE_UP:
            pressed ? outState |= CSTATE_DPAD_UP : outState &= ~CSTATE_DPAD_UP;
            break;
        case SDL_SCANCODE_DOWN:
            pressed ? outState |= CSTATE_DPAD_DOWN : outState &= ~CSTATE_DPAD_DOWN;
            break;
        case SDL_SCANCODE_SPACE:
            pressed ? outState |= CSTATE_A : outState &= ~CSTATE_A;
            break;
        case SDL_SCANCODE_LCTRL:
            pressed ? outState |= CSTATE_B : outState &= ~CSTATE_B;
            break;
        case SDL_SCANCODE_LSHIFT:
            pressed ? outState |= CSTATE_SELECT : outState &= ~CSTATE_SELECT;
            break;
        case SDL_SCANCODE_RETURN:
            pressed ? outState |= CSTATE_START : outState &= ~CSTATE_START;
            break;
        default:
            break;
        }
    }

    void Update() {
        previousState = currentState;
    }

    void ProcessEvent(const SDL_Event& event) {
        switch (event.type)
        {
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            if (gameController != nullptr) {
                HandleControllerButtonEvent(event.cbutton, currentState);
            }
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (gameController != nullptr) {
                HandleControllerAxisEvent(event.caxis, currentState);
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            HandleKeyboardEvent(event.key, currentState);
            break;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERDEVICEREMAPPED:
            HandleControllerDeviceEvent(event.cdevice);
            break;
        default:
            break;
        }
    }

    static bool Down(u8 flags, u8 state) {
        return (flags & state) == flags;
    }

    static bool Up(u8 flags, u8 state) {
        return (flags & ~state) == flags;
    }

    bool Down(u8 flags) {
        return Down(flags, currentState);
    }

    bool Up(u8 flags) {
        return Up(flags, currentState);
    }

    bool Pressed(u8 flags) {
        return Down(flags, currentState) && Up(flags, previousState);
    }

    bool Released(u8 flags) {
        return Up(flags, currentState) && Down(flags, previousState);
    }
}