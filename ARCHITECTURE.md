# Pixel Engine Architecture

A data-oriented 2D game engine inspired by the NES, written in C++ with Vulkan compute shaders for authentic retro rendering.

## Design Philosophy 

### Data-Oriented Design (DOD)
- **Memory layout optimization**: Structures designed for cache efficiency
- **Minimal heap allocation**: Prefer stack and pool allocators 
- **Hot code paths**: Critical loops optimized for performance
- **No unnecessary abstraction**: Direct, efficient implementations

### Retro Authenticity  
- **NES-inspired rendering**: Tile-based graphics with palette limitations
- **Software rendering simulation**: Vulkan compute shaders emulate classic PPU behavior
- **Authentic constraints**: Limited colors, tile-based maps, sprite limitations

## Module Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Platform Layer                         │
│                    (src/platform/)                         │
├─────────────────────────────────────────────────────────────┤
│                      Game Logic                            │
│                     (src/game/)                            │
├─────────────────┬─────────────────┬─────────────────────────┤
│   Core Utils    │   Rendering     │      Audio              │
│  (src/core/)    │ (src/rendering/)│   (src/audio/)          │
├─────────────────┴─────────────────┴─────────────────────────┤
│                   Asset Management                         │
│                  (src/assets/)                             │
├─────────────────────────────────────────────────────────────┤
│                      Editor                                │
│                   (src/editor/)                            │ 
└─────────────────────────────────────────────────────────────┘
```

## Module Descriptions

### Platform (`src/platform/`)
**Purpose**: Application entry point and platform abstraction  
**Dependencies**: All other modules  
**Key Files**: `main.cpp`

### Game (`src/game/`) 
**Purpose**: Core gameplay logic, entities, and game systems  
**Dependencies**: Core, Rendering, Audio, Assets  
**Public API**: `game_api.h` → `game.h`  
**Key Systems**: Entity management, collision, game state, UI

### Core (`src/core/`)
**Purpose**: Fundamental engine utilities and systems  
**Dependencies**: SDL2 (minimal)  
**Public API**: `core.h`  
**Key Systems**: Input handling, memory utilities, timing

### Rendering (`src/rendering/`)
**Purpose**: Vulkan-based NES-style rendering system  
**Dependencies**: Vulkan, Core  
**Public API**: `rendering_api.h` → `rendering.h`  
**Key Systems**: Compute shader rendering, CHR/palette management

### Audio (`src/audio/`)
**Purpose**: SDL2-based audio system  
**Dependencies**: SDL2  
**Public API**: `audio_api.h` → `audio.h`  
**Key Systems**: Sound effects, music playback

### Assets (`src/assets/`)
**Purpose**: Asset loading, management, and serialization  
**Dependencies**: nlohmann_json  
**Key Systems**: Asset packer, runtime loading, type definitions

### Editor (`src/editor/`)
**Purpose**: ImGui-based level and asset editor  
**Dependencies**: ImGui, All other modules  
**Conditional**: Only built when `ENABLE_EDITOR=ON`

## Build System

- **CMake 3.22+**: Modern CMake with FetchContent for dependencies
- **Modular structure**: Each module can be built as separate library in future
- **Conditional compilation**: Editor functionality properly isolated
- **Dependency management**: Clear module boundaries and include paths

## Performance Considerations

### Hot Paths
- Game update loop: 60fps target, optimized entity systems
- Rendering: Compute shader batching, minimal CPU overhead
- Input: Direct SDL event processing, no abstraction layers

### Memory Management
- **Pool allocators**: Fixed-size entity pools for predictable allocation
- **Stack preference**: Minimize heap allocation in gameplay code
- **Data locality**: Structures laid out for cache efficiency

### Editor vs Runtime
- **Editor code**: Can use STL, dynamic allocation for convenience
- **Runtime code**: Performance-critical, follows DOD principles strictly
- **Clear separation**: `#ifdef EDITOR` blocks prevent editor code from affecting runtime

## Development Guidelines

### Code Style
- **C-style C++**: Avoid templates, inheritance, heavy STL usage in hot paths
- **Data-first**: Design data structures before algorithms
- **Explicit**: Prefer explicit casts and clear intentions
- **Minimal**: Only include what you need, avoid header bloat

### Module Boundaries
- **Use public APIs**: Include `*_api.h` headers between modules
- **Avoid cross-dependencies**: Each module should have clear responsibilities
- **Keep shared data in assets/**: Common data types live in `src/assets/shared/`

### Testing Strategy
- **Build validation**: Ensure all configurations build successfully
- **Asset pipeline**: Test asset loading and packing
- **Performance monitoring**: Profile hot paths regularly

## Future Improvements

### Near Term
- [ ] Convert modules to separate static libraries
- [ ] Add module dependency validation
- [ ] Improve header include optimization
- [ ] Add performance profiling hooks

### Long Term  
- [ ] Platform abstraction layer
- [ ] Modular renderer backends
- [ ] Plugin system for game logic
- [ ] Advanced memory debugging tools

---

This architecture balances performance, maintainability, and authenticity to create a modern engine with classic gaming constraints.