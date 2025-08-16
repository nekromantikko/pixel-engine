#pragma once
#include "typedef.h"
#include "asset_types.h"

struct GameConfig {
	ChrBankHandle uiBankHandle;
	ChrBankHandle mapBankHandle;
	ActorPrototypeHandle playerPrototypeHandle;
	ActorPrototypeHandle playerOverworldPrototypeHandle;
	ActorPrototypeHandle xpRemnantPrototypeHandle;
	
	// TODO: Add this for complete tile debris functionality
	// ActorPrototypeHandle tileDebrisEffect;

	OverworldHandle overworldHandle;
};

namespace Game {
	void Initialize();
	void Free();
	const GameConfig& GetConfig();
	
	void Update(r64 dt);
}
