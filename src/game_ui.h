#pragma once
#include "typedef.h"

struct BarState {
	u16 red;
	u16 yellow;
};

struct ExpCounterState {
	u16 exp;
};

namespace Game {
	namespace UI {
		void DrawPlayerHealthBar(u16 maxHealth);
		void DrawPlayerStaminaBar(u16 maxStamina);
		void DrawExpCounter();

		void SetPlayerDisplayHealth(u16 health);
		void SetPlayerDisplayStamina(u16 stamina);
		void SetPlayerDisplayExp(u16 exp);
	}
}