# pixel-engine

A retro inspired game project written in C++, using Vulkan for NES-style "software rendering" with compute shaders, SDL2 for input/audio and Dear ImGui for editor GUI.

# Inspiration
I've written 2D pixel games in the past, and always struggled with rendering, as polygon rendering is not very well suited for pixel art. It requires all kinds of hacks to prevent texture bleeding, keeping everything pixel aligned etc. In general it's also totally overkill for drawing pixel art. Why go through all this headache when I can just write a compute shader that does blitting the old-fashioned way? The NES was my main inspiration, but my renderer allows more colors per tile and a higher resolution, so it's not fully 8-bit but it is based on how the NES renders its graphics. I edit my graphics using YY-CHR, a tool for editing NES graphics. The main color palette is procedurally generated.
For the final image, I apply a CRT shader originally by Timothy Lottes (https://www.shadertoy.com/view/XsjSzR)

# Building
## Requirements
- CMake 3.22.1 or higher
- Vulkan 1.2 development libraries
- SDL2 development libraries  
- GLSL validator (glslangValidator)

## Windows
- Copy SDL2.dll and assets.npak to build directory

## Linux
- Install dependencies: `sudo apt install libvulkan-dev libsdl2-dev glslang-tools`
- Copy assets.npak to build directory

## Build Commands
- Use cmake to configure and build
- Use ENABLE_EDITOR option to disable/enable editor
- Use ENABLE_AVX option to enable AVX2 optimizations on Linux (Windows: auto-enabled)
- Example: `cmake -DENABLE_EDITOR=ON -DENABLE_AVX=ON ..` then `make`

## Performance Features
- **AVX2 Support**: Automatic detection and use of AVX2 instructions for improved performance
- **Runtime CPU Detection**: Safely falls back to standard implementations on older CPUs
- **Cross-platform**: AVX optimizations work on Windows, Linux, and macOS

# Videos:

Gameplay in an early stage of development:

https://github.com/user-attachments/assets/f80a977d-ebd0-4e1d-b281-29598b344289

Emulating the NES APU:

https://github.com/user-attachments/assets/f5381c08-9041-402b-bf1c-d73d582101c6




# Currently working on:
- Playable demo
