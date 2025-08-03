# Core Module

## Purpose 
Core engine systems and utilities that provide fundamental functionality used throughout the engine.

## Components
- **Input System** (`input.h/cpp`): Handles SDL input events, keyboard/controller state management
- **Memory Utilities** (`fixed_hash_map.h`): Data-oriented hash map implementation for performance  
- **Timing** (`nes_timing.h`): NES-accurate timing constants and utilities

## Design Principles
- **Data-oriented**: Minimize heap allocations, prefer stack/pool allocations
- **Performance-first**: Hot code paths optimized for cache efficiency
- **No STL**: Custom containers and utilities to maintain control over memory layout

## Dependencies
- SDL2 (for input handling)
- Standard C library only

## Usage
Include the appropriate header files. Most systems are initialized/updated from the main game loop.

## Future Improvements
- Consider making this a separate static library
- Add memory pool allocators
- Platform abstraction layer for input