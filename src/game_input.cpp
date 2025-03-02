#include "game_input.h"
#include "input.h"

static u8 currentInput = BUTTON_NONE;
static u8 previousInput = BUTTON_NONE;

bool Game::Input::ButtonDown(u8 flags) {
	return (flags & currentInput) == flags;
}
bool Game::Input::ButtonUp(u8 flags) {
	return (flags & ~currentInput) == flags;
}
bool Game::Input::ButtonPressed(u8 flags) {
	return ((currentInput & ~previousInput) & flags) == flags;
}
bool Game::Input::ButtonReleased(u8 flags) {
	return ((~currentInput & previousInput) & flags) == flags;
}
// Returns true if any of the buttons in the mask are down
bool Game::Input::AnyButtonDown(u8 mask) {
	return (mask & currentInput) != 0;
}
// Returns true if any of the buttons in the mask are up
bool Game::Input::AnyButtonUp(u8 mask) {
	return (mask & ~currentInput) != 0;
}
// Returns true if any of the buttons in the mask were pressed
bool Game::Input::AnyButtonPressed(u8 mask) {
	return ((currentInput & ~previousInput) & mask) != 0;
}
// Returns true if any of the buttons in the mask were released
bool Game::Input::AnyButtonReleased(u8 mask) {
	return ((~currentInput & previousInput) & mask) != 0;
}

void Game::Input::Update() {
	previousInput = currentInput;
	currentInput = ::Input::GetControllerState();
}