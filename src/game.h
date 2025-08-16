#pragma once
#include "typedef.h"
#include "asset_types.h"

struct GameConfig {
	ChrBankHandle uiBankHandle;
	ChrBankHandle mapBankHandle;
	ActorPrototypeHandle playerPrototypeHandle;
	ActorPrototypeHandle playerOverworldPrototypeHandle;
	ActorPrototypeHandle xpRemnantPrototypeHandle;

	OverworldHandle overworldHandle;
};

namespace Game {
	void Initialize();
	void Free();
	const GameConfig& GetConfig();
	
	void Update(r64 dt);
	
	// Game control
	bool ShouldExit();
	void RequestExit();
}
