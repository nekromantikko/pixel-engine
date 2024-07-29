#pragma once
#include <cstdlib>
#include "typedef.h"

namespace Audio {
	struct AudioContext;

	AudioContext* CreateAudioContext();
	void FreeAudioContext(AudioContext* pAudioContext);

	void Update(r64 dt, AudioContext* pContext);
	void PeekAudioBuffer(AudioContext* pContext, u8* outSamples, u32 count);
	void ReadAudioBuffer(AudioContext* pContext, u8* outSamples, u32 count);
	void WriteAudioBuffer(AudioContext* pContext, u8* samples, u32 count);
}