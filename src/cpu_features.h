#pragma once
#include "typedef.h"

namespace CpuFeatures {
    // CPU feature flags
    struct Features {
        bool avx = false;
        bool avx2 = false;
        bool sse2 = false;
        bool sse41 = false;
        bool sse42 = false;
    };

    // Initialize and detect CPU features
    void Initialize();

    // Get detected CPU features
    const Features& GetFeatures();

    // Check if AVX is supported at runtime
    bool HasAVXSupport();

    // Check if AVX2 is supported at runtime
    bool HasAVX2Support();

    // Check if AVX is available both at compile time and runtime
    bool IsAVXAvailable();
}