#pragma once
#include "typedef.h"

namespace Game {
	namespace UI {
		void DrawPlayerHealthBar(s16 maxHealth);
		void DrawPlayerStaminaBar(s16 maxStamina);
		void DrawExpCounter();

		void SetPlayerDisplayHealth(s16 health);
		void SetPlayerDisplayStamina(s16 stamina);
		void SetPlayerDisplayExp(s16 exp);

		void Update();
	}
}