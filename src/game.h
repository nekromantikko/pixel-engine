#pragma once
#include "typedef.h"

namespace Game {
	void Initialize();
	void Free();
	
	void Update(r64 dt);
	bool ShouldExit();
}
