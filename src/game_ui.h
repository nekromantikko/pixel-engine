#pragma once
#include "typedef.h"

struct HealthBarState {
	u16 red;
	u16 yellow;
};

struct ExpCounterState {
	u16 exp;
};

namespace Game {
	namespace UI {
		void DrawPlayerHealthBar(u16 maxHealth);
		void DrawExpCounter();

		void SetPlayerDisplayHealth(u16 health);
		void SetPlayerDisplayExp(u16 exp);
	}
}