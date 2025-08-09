#ifdef HEADLESS

#include "audio.h"
#include "core_types.h"
#include <cstring>

// Headless audio - provides the same interface but does nothing

namespace Audio {
    void Init() {
        // No-op in headless mode
    }
    
    void Free() {
        // No-op in headless mode
    }

    void WriteChannel(u32 channel, u8 address, u8 data) {
        // No-op in headless mode
    }

    void PlayMusic(SoundHandle musicHandle, bool loop) {
        // No-op in headless mode
    }
    
    void StopMusic() {
        // No-op in headless mode
    }
    
    void PlaySFX(SoundHandle soundHandle) {
        // No-op in headless mode
    }

#ifdef EDITOR
    void ReadChannel(u32 channel, void* outData) {
        // No-op in headless mode
        if (outData) {
            memset(outData, 0, sizeof(u32)); // Return zeroed data
        }
    }
    
    void ReadDebugBuffer(u8* outSamples, u32 count) {
        // No-op in headless mode
        if (outSamples && count > 0) {
            memset(outSamples, 0, count);
        }
    }
#endif
}

#endif // HEADLESS