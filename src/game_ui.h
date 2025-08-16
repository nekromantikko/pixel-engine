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

		// Menu system
		void DrawText(const char* text, const glm::ivec2& position, u8 palette = 0x1);
		void DrawMenuItem(const char* text, const glm::ivec2& position, bool selected, u8 palette = 0x1);
		void DrawSlider(const char* label, r32 value, const glm::ivec2& position, bool selected, u8 palette = 0x1);
	}
}