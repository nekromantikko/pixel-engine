#include "input.h"
#include "system.h"
#include <SDL.h>
#include <cmath>

namespace Input {
    //TODO: multiple controller support
    SDL_GameController* gameController = nullptr;
    SDL_Haptic* haptic = nullptr;

    ControllerState currentState = None;
    ControllerState previousState = None;

    void InitController() {
        if (SDL_NumJoysticks()) {
            gameController = SDL_GameControllerOpen(0);
            haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(gameController));
            SDL_HapticRumbleInit(haptic);
        }
        else gameController = nullptr;
    }

    void FreeController() {
        if (gameController) {
            SDL_GameControllerClose(gameController);
        }
        if (haptic) {
            SDL_HapticRumbleStop(haptic);
            SDL_HapticClose(haptic);
        }
    }

    void HandleControllerButtonEvent(SDL_ControllerButtonEvent& event, u8& outState)
    {
        bool pressed = (event.state == SDL_PRESSED);

        switch (event.button)
        {
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            pressed ? outState |= DPadLeft : outState &= ~DPadLeft;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            pressed ? outState |= DPadRight : outState &= ~DPadRight;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            pressed ? outState |= DPadUp : outState &= ~DPadUp;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            pressed ? outState |= DPadDown : outState &= ~DPadDown;
            break;
        case SDL_CONTROLLER_BUTTON_A:
            pressed ? outState |= A : outState &= ~A;
            break;
        case SDL_CONTROLLER_BUTTON_B:
            pressed ? outState |= B : outState &= ~B;
            break;
        case SDL_CONTROLLER_BUTTON_BACK:
            pressed ? outState |= Select : outState &= ~Select;
            break;
        case SDL_CONTROLLER_BUTTON_START:
            pressed ? outState |= Start : outState &= ~Start;
            break;
        default:
            break;
        }
    }

    void HandleControllerAxisEvent(SDL_ControllerAxisEvent& event, u8& outState)
    {
        // TODO
    }

    void HandleControllerDeviceEvent(SDL_ControllerDeviceEvent& event)
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

    void Poll() {
        previousState = currentState;
        u8 stateByte = (u8)previousState;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                if (gameController != nullptr) {
                    HandleControllerButtonEvent(event.cbutton, stateByte);
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (gameController != nullptr) {
                    HandleControllerAxisEvent(event.caxis, stateByte);
                }
                break;
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
            case SDL_CONTROLLERDEVICEREMAPPED:
                HandleControllerDeviceEvent(event.cdevice);
            default:
                break;
            }
        }

        currentState = (ControllerState)stateByte;
	}

    bool Down(ControllerState flags, ControllerState state) {
        return (flags & state) == flags;
    }

    bool Up(ControllerState flags, ControllerState state) {
        return (flags & ~state) == flags;
    }

    bool Down(ControllerState flags) {
        return Down(flags, currentState);
    }

    bool Up(ControllerState flags) {
        return Up(flags, currentState);
    }

    bool Pressed(ControllerState flags) {
        return Down(flags, currentState) && Up(flags, previousState);
    }

    bool Released(ControllerState flags) {
        return Up(flags, currentState) && Down(flags, previousState);
    }
}