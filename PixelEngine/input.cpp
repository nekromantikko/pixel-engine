#include "input.h"
#include "system.h"
#include <SDL.h>
#include <cmath>

namespace Input {
    //TODO: multiple controller support
    SDL_GameController* gameController = nullptr;
    SDL_Haptic* haptic = nullptr;

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
            pressed ? outState |= Left : outState &= ~Left;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            pressed ? outState |= Right : outState &= ~Right;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            pressed ? outState |= Up : outState &= ~Up;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            pressed ? outState |= Down : outState &= ~Down;
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
        /*r32 sensitivity = 0.2f;
        r32 value = event.value / 32767.0f;
        switch (event.axis)
        {
        case SDL_CONTROLLER_AXIS_LEFTX:
            abs(value) <= sensitivity ? outState &= ~(Left | Right) : value > 0 ? outState |= Right : outState |= Left;
            break;
        case SDL_CONTROLLER_AXIS_LEFTY:
            abs(value) <= sensitivity ? outState &= ~(Up | Down) : value > 0 ? outState |= Up : outState |= Down;
            break;
        default:
            break;
        }*/
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

    ControllerState PollInput(ControllerState previousState) {
        u8 buttonState = (u8)previousState;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                if (gameController != nullptr) {
                    HandleControllerButtonEvent(event.cbutton, buttonState);
                }
                break;
            case SDL_CONTROLLERAXISMOTION:
                /*if (gameController != nullptr) {
                    HandleControllerAxisEvent(event.caxis, axisState);
                }*/
                break;
            case SDL_CONTROLLERDEVICEADDED:
            case SDL_CONTROLLERDEVICEREMOVED:
            case SDL_CONTROLLERDEVICEREMAPPED:
                HandleControllerDeviceEvent(event.cdevice);
            default:
                break;
            }
        }


        return (ControllerState)(buttonState);
	}
}