# Memory Optimization Guide for Pixel Engine

## Problem Analysis

The pixel engine was using **300MB+ memory** according to Visual Studio debugger, which is excessive for a pixel art game. Investigation revealed the main culprits:

## Major Memory Consumers

### 1. Vulkan Validation Layers (100-200MB)
- **Impact**: Largest single memory consumer
- **Cause**: Always enabled in previous builds
- **Solution**: Made conditional via `ENABLE_VULKAN_VALIDATION` option

### 2. Arena Allocations (16MB → 8MB)
- **Impact**: Fixed 16MB allocation regardless of usage
- **Cause**: Oversized arenas for pixel art game needs
- **Solution**: Reduced defaults and made configurable

### 3. GPU Driver Overhead (50-200MB)
- **Impact**: Unavoidable but variable by driver
- **Cause**: GPU driver memory management
- **Solution**: Use minimal Vulkan features, no validation in production

### 4. Graphics Buffers (~1.4MB)
- **Impact**: Minimal for current resolution
- **Breakdown**:
  - Color images: 512x288x4 bytes x2 = 1.12MB
  - Sprite buffers: 64 sprites/line x288 lines x4 bytes x2 = 0.14MB
  - Palette: 128 colors x4 bytes = 0.5KB

## Optimization Options

### Build Configuration

#### Minimal Memory Build (Production)
```bash
cmake -DENABLE_EDITOR=OFF \
      -DBUILD_ASSETS=OFF \
      -DENABLE_VULKAN_VALIDATION=OFF \
      -DPERMANENT_ARENA_SIZE_MB=2 \
      -DASSET_ARENA_SIZE_MB=1 \
      -DSCRATCH_ARENA_SIZE_MB=1 \
      ..
```

#### Development Build (with debugging)
```bash
cmake -DENABLE_EDITOR=ON \
      -DBUILD_ASSETS=ON \
      -DENABLE_VULKAN_VALIDATION=ON \
      -DPERMANENT_ARENA_SIZE_MB=8 \
      -DASSET_ARENA_SIZE_MB=4 \
      -DSCRATCH_ARENA_SIZE_MB=4 \
      ..
```

### Arena Size Guidelines

| Arena Type | Minimal | Default | Development | Purpose |
|------------|---------|---------|-------------|---------|
| Permanent  | 1-2MB   | 4MB     | 8MB         | Long-lived game objects |
| Assets     | 1MB     | 2MB     | 4MB         | Loaded game assets |
| Scratch    | 1MB     | 2MB     | 4MB         | Temporary allocations |

### Runtime Options

1. **Vulkan Validation**
   - Development: Enable for debugging
   - Production: Always disable (saves 100-200MB)

2. **Debug Logging**
   - Consider reducing verbosity in release builds
   - Profile if excessive logging impacts memory

## Additional Optimizations

### GPU Memory Optimizations

1. **Texture Compression**
   - Consider compressed formats for large textures
   - Current palette-based approach is already efficient

2. **Buffer Sizing**
   - Reduce `MAX_SPRITES_PER_SCANLINE` if not needed (currently 64)
   - Use 16-bit indices instead of 32-bit where possible

3. **Single Buffering**
   - Consider single buffering in `COMMAND_BUFFER_COUNT` for memory-constrained environments
   - Trade latency for 50% GPU memory reduction

### System Memory Optimizations

1. **Asset Streaming**
   - Load assets on-demand instead of all at startup
   - Implement asset LRU cache in asset arena

2. **Pool Sizes**
   - Audit Pool template usage for oversized capacity values
   - Reduce MAX_* constants based on actual game needs

## Memory Profiling

### Built-in Tools

The engine includes memory profiling tools:

```cpp
#include "memory_profiler.h"

// Initialize profiling
MemoryProfiler::Init();

// Log memory at specific points
MemoryProfiler::LogMemoryPoint("AFTER_ASSET_LOADING");

// Get final report
MemoryProfiler::ReportFinal();
```

### External Tools

1. **Valgrind** (Linux)
   ```bash
   valgrind --tool=massif ./pixelengine
   ```

2. **Windows Performance Tools**
   - Use Process Monitor for detailed analysis
   - Visual Studio Diagnostic Tools

3. **GPU Memory Tools**
   - Vulkan validation layers (development only)
   - GPU vendor tools (NVIDIA Nsight, AMD GPU PerfStudio)

## Expected Results

| Configuration | Expected Memory Usage |
|---------------|----------------------|
| Production (optimized) | 15-30MB |
| Development (validation) | 150-250MB |
| Original (unoptimized) | 300MB+ |

## Recommendations

1. **Always use production build** for performance testing
2. **Profile regularly** during development to catch regressions
3. **Monitor arena utilization** - reduce sizes if consistently under-utilized
4. **Consider platform differences** - mobile/embedded may need smaller arenas
5. **Test without validation layers** before reporting memory issues