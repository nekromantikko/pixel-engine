# Destructible Blocks Implementation

This document describes the implementation of destructible blocks in the pixel engine, similar to the blocks in Super Mario Bros.

## Overview

When a projectile hits a destructible block, the block disappears and spawns four particle sprites that fly outward in different directions, creating the illusion that the block broke into pieces.

## Changes Made

### 1. Core Types (src/core_types.h)
- Added `TILE_DESTRUCTIBLE = 2` to `TilesetTileType` enum
- Updated `METATILE_TYPE_NAMES` to include "Destructible"
- Added `tileCoord` field to `HitResult` to track which tile was hit

### 2. Effect System (src/actor_data.h, src/effect.cpp)
- Added `EFFECT_TYPE_TILE_DEBRIS` for the particle effects
- Implemented `UpdateTileDebris()` function with gravity and physics
- Added debris effect to the effect function tables

### 3. Collision System (src/collision.cpp)
- Modified both horizontal and vertical collision detection
- Destructible tiles now block movement like solid tiles
- Store tile coordinates in HitResult when hit

### 4. Bullet System (src/bullet.cpp)
- Added `HandleBulletTileCollision()` helper function
- Regular bullets destroy destructible tiles and die
- Grenades destroy destructible tiles and continue (ricochet)

### 5. Game State (src/game_state.cpp, src/game_state.h)
- Added `Game::DestroyTileAt()` function
- Removes tile from tilemap (sets to TILE_EMPTY)
- Spawns 4 debris particles in diagonal directions

## Usage

1. **Set up destructible tiles**: In your tilemap editor, set tiles to use the TILE_DESTRUCTIBLE type.

2. **Configure debris effect**: Create an actor prototype for EFFECT_TYPE_TILE_DEBRIS with:
   - Appropriate sprite/animation for the debris pieces
   - Lifetime duration (how long particles last)
   - Optional sound effect

3. **Bullets automatically handle destruction**: When any bullet hits a destructible tile, the tile is removed and debris spawns.

## Particle Physics

The debris particles:
- Spawn at the center of the destroyed tile
- Fly outward in 4 diagonal directions (like Super Mario Bros)
- Have initial velocity of 0.15 units in X and Y directions
- Apply gravity (0.01 units/frame) to fall down
- Slow down horizontal movement over time (multiply by 0.98 each frame)
- Remove themselves when their lifetime expires

## Example Integration

```cpp
// In your asset loading code:
ActorPrototypeHandle debrisEffect = LoadActorPrototype("tile_debris.json");

// The debris effect would have:
// - Type: ACTOR_TYPE_EFFECT, EFFECT_TYPE_TILE_DEBRIS  
// - Small sprite for debris pieces
// - Lifetime: 60-120 frames
// - Optional destruction sound
```

The system is now ready to use - just configure the asset prototypes and the destructible blocks will work automatically when bullets hit them!