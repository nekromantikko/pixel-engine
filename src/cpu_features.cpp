#include "cpu_features.h"
#include <cstring>

#ifdef PLATFORM_WINDOWS
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace CpuFeatures {
    static Features s_features;
    static bool s_initialized = false;

    static void DetectCpuFeatures() {
#ifdef PLATFORM_WINDOWS
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        
        // Check CPUID feature flags
        const int ecx = cpuInfo[2];
        const int edx = cpuInfo[3];
        
        s_features.sse2 = (edx & (1 << 26)) != 0;
        s_features.sse41 = (ecx & (1 << 19)) != 0;
        s_features.sse42 = (ecx & (1 << 20)) != 0;
        s_features.avx = (ecx & (1 << 28)) != 0;
        
        // Check for AVX2 support with extended feature flags
        if (s_features.avx) {
            __cpuidex(cpuInfo, 7, 0);
            const int ebx = cpuInfo[1];
            s_features.avx2 = (ebx & (1 << 5)) != 0;
        }
        
        // Check if OS supports AVX (XGETBV instruction)
        if (s_features.avx) {
            const unsigned long long xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
            s_features.avx = (xcrFeatureMask & 0x6) == 0x6; // Check if XMM and YMM state are enabled
        }
#else
        // Linux/GCC implementation using __get_cpuid
        unsigned int eax, ebx, ecx, edx;
        
        // Check basic features
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            s_features.sse2 = (edx & (1 << 26)) != 0;
            s_features.sse41 = (ecx & (1 << 19)) != 0;
            s_features.sse42 = (ecx & (1 << 20)) != 0;
            s_features.avx = (ecx & (1 << 28)) != 0;
            
            // Check if OS supports AVX (XGETBV instruction)
            if (s_features.avx) {
                // Check if XGETBV is supported
                if ((ecx & (1 << 27)) != 0) {
                    unsigned int xcr0;
                    __asm__("xgetbv" : "=a"(xcr0) : "c"(0) : "edx");
                    bool osSupportsAVX = (xcr0 & 0x6) == 0x6; // Check if XMM and YMM state are enabled
                    if (!osSupportsAVX) {
                        s_features.avx = false;
                    }
                } else {
                    s_features.avx = false;
                }
            }
        }
        
        // Check for AVX2 support (only if AVX is supported)
        if (s_features.avx && __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            s_features.avx2 = (ebx & (1 << 5)) != 0;
        }
#endif
    }

    void Initialize() {
        if (!s_initialized) {
            DetectCpuFeatures();
            s_initialized = true;
        }
    }

    const Features& GetFeatures() {
        Initialize();
        return s_features;
    }

    bool HasAVXSupport() {
        Initialize();
        return s_features.avx;
    }

    bool HasAVX2Support() {
        Initialize();
        return s_features.avx2;
    }

    bool IsAVXAvailable() {
#ifdef USE_AVX
        // Since the actual code uses AVX2 instructions, check for AVX2 support
        // Only return true if both compile-time and runtime support is available
        Initialize();
        return s_features.avx2;
#else
        // If not compiled with AVX support, always return false
        return false;
#endif
    }
}