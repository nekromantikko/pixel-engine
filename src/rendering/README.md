# Rendering Module

## Purpose
Vulkan-based rendering system implementing NES-style "software rendering" using compute shaders.

## Architecture
- **Software Rendering**: Uses compute shaders to emulate NES PPU behavior
- **Character (CHR) Data**: Tile-based graphics system similar to NES
- **Palette System**: Limited color palette per sprite/background tile
- **CRT Effects**: Post-processing for authentic retro look

## Components
- **Vulkan Backend** (`rendering_vulkan.cpp`): Core Vulkan implementation
- **Utilities** (`rendering_util.h/cpp`): Helper functions and data structures  
- **Shader Management** (`shader.h/cpp`): SPIR-V shader loading and management
- **Public API** (`rendering.h`): Clean interface for the rest of the engine

## Design Principles  
- **Data-oriented**: Rendering data laid out for optimal GPU access patterns
- **Performance**: Minimal CPU overhead, batch operations
- **Retro Accuracy**: Faithful to NES rendering limitations and style

## Dependencies
- Vulkan 1.2+
- GLSL shader compiler
- SDL2 (for surface creation)

## Shaders
Located in `../shaders/`:
- `software.comp`: Main software rendering compute shader
- `textured_crt.frag`: CRT filter post-processing  
- `quad.vert`: Fullscreen quad vertex shader

## Usage
```cpp
Rendering::CreateContext();
Rendering::CreateSurface(window);
Rendering::Init();

// Per frame:
Rendering::BeginFrame();
Rendering::BeginRenderPass();
// ... draw calls ...
Rendering::EndFrame();
```