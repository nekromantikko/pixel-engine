#pragma once
#define GLM_FORCE_RADIANS
#include <glm.hpp>

namespace Game {
	bool OpenDialog(const glm::ivec2& offset, const glm::ivec2& size, const glm::ivec2& initialSize = {0,0});
	void CloseDialog();
	bool UpdateDialog();
	bool IsDialogActive();
	bool IsDialogOpen();
	glm::ivec4 GetDialogInnerBounds();
}