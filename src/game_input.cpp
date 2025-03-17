#include "game_input.h"
#include "input.h"

static u16 currentInput = BUTTON_NONE;
static u16 previousInput = BUTTON_NONE;

bool Game::Input::ButtonDown(u16 flags) {
	return (flags & currentInput) == flags;
}
bool Game::Input::ButtonUp(u16 flags) {
	return (flags & ~currentInput) == flags;
}
bool Game::Input::ButtonPressed(u16 flags) {
	return ((currentInput & ~previousInput) & flags) == flags;
}
bool Game::Input::ButtonReleased(u16 flags) {
	return ((~currentInput & previousInput) & flags) == flags;
}
// Returns true if any of the buttons in the mask are down
bool Game::Input::AnyButtonDown(u16 mask) {
	return (mask & currentInput) != 0;
}
// Returns true if any of the buttons in the mask are up
bool Game::Input::AnyButtonUp(u16 mask) {
	return (mask & ~currentInput) != 0;
}
// Returns true if any of the buttons in the mask were pressed
bool Game::Input::AnyButtonPressed(u16 mask) {
	return ((currentInput & ~previousInput) & mask) != 0;
}
// Returns true if any of the buttons in the mask were released
bool Game::Input::AnyButtonReleased(u16 mask) {
	return ((~currentInput & previousInput) & mask) != 0;
}

u16 Game::Input::GetCurrentState() {
	return currentInput;
}

void Game::Input::Update() {
	previousInput = currentInput;
	currentInput = ::Input::GetControllerState();
}