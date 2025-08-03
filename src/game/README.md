# Game Module

## Purpose
Contains all game-specific logic, entities, and gameplay systems. This is where the actual game mechanics live.

## Components

### Core Game Systems
- **Game Loop** (`game.h/cpp`): Main game update and initialization
- **Game State** (`game_state.h/cpp`): State management, scene transitions
- **Game Input** (`game_input.h/cpp`): Input interpretation for gameplay
- **Game Rendering** (`game_rendering.h/cpp`): Game-specific rendering calls
- **Game UI** (`game_ui.h/cpp`): In-game user interface

### Entity Systems  
- **Actors** (`actors.h/cpp`): Generic entity system with data-driven behavior
- **Player** (`player.h/cpp`): Player character logic and controls
- **Enemies** (`enemy.h/cpp`): AI behavior and enemy entities
- **Bullets** (`bullet.h/cpp`): Projectile physics and collision
- **Pickups** (`pickup.h/cpp`): Collectible items and power-ups  
- **Interactables** (`interactable.h/cpp`): Objects the player can interact with
- **Spawners** (`spawner.h/cpp`): Entity spawning system
- **Effects** (`effect.h/cpp`): Visual effects and particles

### Game Systems
- **Collision** (`collision.h/cpp`): Physics and collision detection
- **Dialog** (`dialog.h/cpp`): Text dialogue system
- **Tilemap** (`tilemap.h/cpp`): Level/map representation  
- **Coroutines** (`coroutines.h/cpp`): Cooperative multitasking for game logic

## Design Principles
- **Data-driven**: Entity behavior defined by data, not inheritance
- **Performance-critical**: Hot update loops optimized for 60fps
- **Memory-efficient**: Pool allocators, minimal dynamic allocation
- **Modular**: Systems can be updated independently

## Dependencies
- Core module (input, utilities)
- Rendering module (for drawing)
- Audio module (for sound effects)
- Assets module (for game data)

## Architecture Notes
The game uses an entity-component-like system where actors have behavior defined by prototypes loaded from assets. This allows for data-driven gameplay without the overhead of traditional ECS systems.