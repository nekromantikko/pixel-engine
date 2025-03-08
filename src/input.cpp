#include "input.h"
#include "game_input.h"
#include <SDL.h>

//TODO: multiple controller support
static SDL_GameController* gameController = nullptr;
static SDL_Haptic* haptic = nullptr;

static u16 controllerState = BUTTON_NONE;

#pragma region Internal functions
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

static void HandleControllerButtonEvent(const SDL_ControllerButtonEvent& event, u16& outState)
{
    const bool pressed = (event.state == SDL_PRESSED);

    switch (event.button)
    {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        pressed ? outState |= BUTTON_DPAD_LEFT : outState &= ~BUTTON_DPAD_LEFT;
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        pressed ? outState |= BUTTON_DPAD_RIGHT : outState &= ~BUTTON_DPAD_RIGHT;
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        pressed ? outState |= BUTTON_DPAD_UP : outState &= ~BUTTON_DPAD_UP;
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        pressed ? outState |= BUTTON_DPAD_DOWN : outState &= ~BUTTON_DPAD_DOWN;
        break;
    case SDL_CONTROLLER_BUTTON_A:
        pressed ? outState |= BUTTON_A : outState &= ~BUTTON_A;
        break;
    case SDL_CONTROLLER_BUTTON_B:
        pressed ? outState |= BUTTON_B : outState &= ~BUTTON_B;
        break;
    case SDL_CONTROLLER_BUTTON_X:
        pressed ? outState |= BUTTON_X : outState &= ~BUTTON_X;
        break;
    case SDL_CONTROLLER_BUTTON_Y:
        pressed ? outState |= BUTTON_Y : outState &= ~BUTTON_Y;
        break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        pressed ? outState |= BUTTON_L : outState &= ~BUTTON_L;
        break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        pressed ? outState |= BUTTON_R : outState &= ~BUTTON_R;
        break;
    case SDL_CONTROLLER_BUTTON_BACK:
        pressed ? outState |= BUTTON_SELECT : outState &= ~BUTTON_SELECT;
        break;
    case SDL_CONTROLLER_BUTTON_START:
        pressed ? outState |= BUTTON_START : outState &= ~BUTTON_START;
        break;
    default:
        break;
    }
}

static void HandleControllerAxisEvent(const SDL_ControllerAxisEvent& event, u16& outState)
{
    const int threshold = 8000; // Deadzone threshold for axis movement
    const bool moved = (abs(event.value) > threshold);

    switch (event.axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX:
        if (moved) {
            if (event.value < 0) {
                outState |= BUTTON_DPAD_LEFT;
            } else {
                outState |= BUTTON_DPAD_RIGHT;
            }
        } else {
            outState &= ~BUTTON_DPAD_LEFT;
            outState &= ~BUTTON_DPAD_RIGHT;
        }
        break;
    case SDL_CONTROLLER_AXIS_LEFTY:
        if (moved) {
            if (event.value < 0) {
                outState |= BUTTON_DPAD_UP;
            } else {
                outState |= BUTTON_DPAD_DOWN;
            }
        } else {
            outState &= ~BUTTON_DPAD_UP;
            outState &= ~BUTTON_DPAD_DOWN;
        }
        break;
    case SDL_CONTROLLER_AXIS_RIGHTX:
        // Handle right stick horizontal movement if needed
        break;
    case SDL_CONTROLLER_AXIS_RIGHTY:
        // Handle right stick vertical movement if needed
        break;
    default:
        break;
    }
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

static void HandleKeyboardEvent(const SDL_KeyboardEvent& event, u16& outState) {
    const bool pressed = (event.state == SDL_PRESSED);

    switch (event.keysym.scancode)
    {
    case SDL_SCANCODE_LEFT:
        pressed ? outState |= BUTTON_DPAD_LEFT : outState &= ~BUTTON_DPAD_LEFT;
        break;
    case SDL_SCANCODE_RIGHT:
        pressed ? outState |= BUTTON_DPAD_RIGHT : outState &= ~BUTTON_DPAD_RIGHT;
        break;
    case SDL_SCANCODE_UP:
        pressed ? outState |= BUTTON_DPAD_UP : outState &= ~BUTTON_DPAD_UP;
        break;
    case SDL_SCANCODE_DOWN:
        pressed ? outState |= BUTTON_DPAD_DOWN : outState &= ~BUTTON_DPAD_DOWN;
        break;
    case SDL_SCANCODE_SPACE:
        pressed ? outState |= BUTTON_A : outState &= ~BUTTON_A;
        break;
    case SDL_SCANCODE_LCTRL:
        pressed ? outState |= BUTTON_B : outState &= ~BUTTON_B;
        break;
    case SDL_SCANCODE_Z:
        pressed ? outState |= BUTTON_X : outState &= ~BUTTON_X;
        break;
    case SDL_SCANCODE_C:
        pressed ? outState |= BUTTON_Y : outState &= ~BUTTON_Y;
        break;
    case SDL_SCANCODE_Q:
        pressed ? outState |= BUTTON_L : outState &= ~BUTTON_L;
        break;
    case SDL_SCANCODE_E:
        pressed ? outState |= BUTTON_R : outState &= ~BUTTON_R;
        break;
    case SDL_SCANCODE_LSHIFT:
        pressed ? outState |= BUTTON_SELECT : outState &= ~BUTTON_SELECT;
        break;
    case SDL_SCANCODE_RETURN:
        pressed ? outState |= BUTTON_START : outState &= ~BUTTON_START;
        break;
    default:
        break;
    }
}
#pragma endregion

#pragma region Public API
void Input::ProcessEvent(const SDL_Event* event) {
    switch (event->type)
    {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        if (gameController != nullptr) {
            HandleControllerButtonEvent(event->cbutton, controllerState);
        }
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (gameController != nullptr) {
            HandleControllerAxisEvent(event->caxis, controllerState);
        }
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        HandleKeyboardEvent(event->key, controllerState);
        break;
    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
    case SDL_CONTROLLERDEVICEREMAPPED:
        HandleControllerDeviceEvent(event->cdevice);
        break;
    default:
        break;
    }
}

u16 Input::GetControllerState() {
    return controllerState;
}
#pragma endregion