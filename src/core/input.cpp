#include "input.h"
#include "game_input.h"
#include "debug.h"
#include <SDL.h>

//TODO: multiple controller support
static SDL_Joystick* pJoystick = nullptr;
static SDL_Haptic* pHaptic = nullptr;

static u16 controllerState = BUTTON_NONE;

#pragma region Internal functions
static bool InitController(s32 index) {
    if (index >= SDL_NumJoysticks()) {
        return false;
    }

    if (index != 0) {
        DEBUG_ERROR("Multiple controllers not supported yet!\n");
        return false;
    }

    if (SDL_IsGameController(index)) {
        SDL_GameController* pController = SDL_GameControllerOpen(index);
        if (!pController) {
            DEBUG_ERROR("Failed to open controller #%d\n", index);
            return false;
        }

        pJoystick = SDL_GameControllerGetJoystick(pController);
    }
    else {
        pJoystick = SDL_JoystickOpen(index);
        if (!pJoystick) {
            DEBUG_ERROR("Failed to open joystick #%d\n", index);
            return false;
        }
    }

    pHaptic = SDL_HapticOpenFromJoystick(pJoystick);
    if (!pHaptic) {
        DEBUG_LOG("Failed to open haptic from joystick #%d\n", index);
    }
    else
    {
        SDL_HapticRumbleInit(pHaptic);
    }

    return true;
}

static bool FreeController(s32 index) {
    if (index >= SDL_NumJoysticks()) {
        return false;
    }

    if (index != 0) {
        return false;
    }

    if (!pJoystick) {
        return false;
    }

    if (SDL_IsGameController(index)) {
        const s32 instanceId = SDL_JoystickInstanceID(pJoystick);
        SDL_GameController* pController = SDL_GameControllerFromInstanceID(instanceId);
        SDL_GameControllerClose(pController);
        DEBUG_LOG("Game controller closed.\n");
    }
    else {
        SDL_JoystickClose(pJoystick);
        DEBUG_LOG("Joystick closed.\n");
    }

    if (pHaptic) {
        SDL_HapticRumbleStop(pHaptic);
        SDL_HapticClose(pHaptic);
        DEBUG_LOG("Haptic device closed.\n");
    }
    
    return true;
}

static void HandleJoystickButtonEvent(const SDL_JoyButtonEvent& event, u16& outState) {
    if (SDL_IsGameController(event.which)) {
        return;
    }

    const bool pressed = (event.state == SDL_PRESSED);

    switch (event.button) {
    case 0:
        pressed ? outState |= BUTTON_X : outState &= ~BUTTON_X;
        break;
    case 1:
        pressed ? outState |= BUTTON_A : outState &= ~BUTTON_A;
        break;
    case 2:
        pressed ? outState |= BUTTON_B : outState &= ~BUTTON_B;
        break;
    case 3:
        pressed ? outState |= BUTTON_Y : outState &= ~BUTTON_Y;
        break;
    case 4:
        pressed ? outState |= BUTTON_L : outState &= ~BUTTON_L;
        break;
    case 5:
        pressed ? outState |= BUTTON_R : outState &= ~BUTTON_R;
        break;
    case 8:
        pressed ? outState |= BUTTON_SELECT : outState &= ~BUTTON_SELECT;
        break;
    case 9:
        pressed ? outState |= BUTTON_START : outState &= ~BUTTON_START;
        break;
    default:
        break;
    }
}

static void HandleJoystickAxisEvent(const SDL_JoyAxisEvent& event, u16& outState) {
    if (SDL_IsGameController(event.which)) {
        return;
    }

    const int threshold = 8000; // Deadzone threshold for axis movement
    const bool moved = (abs(event.value) > threshold);

    switch (event.axis) {
    case 0:
        if (moved) {
            if (event.value < 0) {
                outState |= BUTTON_DPAD_LEFT;
            }
            else {
                outState |= BUTTON_DPAD_RIGHT;
            }
        }
        else {
            outState &= ~BUTTON_DPAD_LEFT;
            outState &= ~BUTTON_DPAD_RIGHT;
        }
        break;
    case 1:
        if (moved) {
            if (event.value < 0) {
                outState |= BUTTON_DPAD_UP;
            }
            else {
                outState |= BUTTON_DPAD_DOWN;
            }
        }
        else {
            outState &= ~BUTTON_DPAD_UP;
            outState &= ~BUTTON_DPAD_DOWN;
        }
        break;
    default:
        break;
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
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        HandleJoystickButtonEvent(event->jbutton, controllerState);
        break;
    case SDL_JOYAXISMOTION:
        HandleJoystickAxisEvent(event->jaxis, controllerState);
        break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        HandleControllerButtonEvent(event->cbutton, controllerState);
        break;
    case SDL_CONTROLLERAXISMOTION:
        HandleControllerAxisEvent(event->caxis, controllerState);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        HandleKeyboardEvent(event->key, controllerState);
        break;
    case SDL_JOYDEVICEADDED:
        InitController(event->jdevice.which);
        break;
    case SDL_JOYDEVICEREMOVED:
        FreeController(event->jdevice.which);
        break;
    case SDL_CONTROLLERDEVICEADDED:
        InitController(event->cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        FreeController(event->cdevice.which);
        break;
    case SDL_CONTROLLERDEVICEREMAPPED:
        break;
    default:
        break;
    }
}

u16 Input::GetControllerState() {
    return controllerState;
}
#pragma endregion