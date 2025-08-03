# Zelda 2 Inspired Test Rooms

This document describes the four new test rooms added to the pixel engine, all inspired by the side-scrolling dungeon design of Zelda II: The Adventure of Link.

## Room Descriptions

### 1. zelda2_castle.room
**Theme**: Castle/Palace Interior  
**Features**:
- Multi-level stone platforms at varying heights
- Connecting staircases between platform levels
- Decorative pillars throughout the room
- Upper-level walkways for vertical gameplay
- Solid walls and ceiling boundaries
- Ground floor with elevated sections

**Gameplay**: Tests vertical movement, jumping between platforms, and exploration of multi-level environments typical of Zelda 2's palace dungeons.

### 2. zelda2_cave.room  
**Theme**: Natural Cave Environment
**Features**:
- Irregular cave walls and ceiling
- Stalactites hanging from ceiling
- Stalagmites rising from floor
- Multiple rock ledges at different elevations
- Natural-looking uneven terrain
- Cave-like organic layout

**Gameplay**: Tests navigation through irregular terrain, jumping across natural formations, and movement in cramped spaces.

### 3. zelda2_sidescroll.room
**Theme**: Side-scrolling Test Course
**Features**:
- Varying ground elevation across the width
- Floating platforms for jumping challenges
- Vertical pillar obstacles
- Strategic gaps requiring precise jumps
- Hanging ceiling obstacles
- Long horizontal layout for testing movement

**Gameplay**: Specifically designed to test the side-scrolling mechanics that were central to Zelda 2's gameplay.

### 4. zelda2_combat.room
**Theme**: Combat Arena
**Features**:
- Central combat arena with raised floor
- Strategic cover pillars
- Elevated sniper/viewing positions
- Multiple engagement areas
- Entrance and exit passages
- Varied elevation for tactical combat

**Gameplay**: Designed for testing combat mechanics, enemy AI navigation, and strategic positioning during battles.

## Technical Details

- All rooms use the standard 128x72 tile format (9,216 tiles total)
- Compatible with existing tileset ID: 3566869058797588725
- Various tile types used: walls, floors, platforms, pillars, decorative elements
- Each room includes proper metadata files with unique GUIDs
- JSON structure matches existing room format exactly

## Integration

These rooms integrate seamlessly with the existing asset system and will be packed into the assets.npak file during build. They can be loaded and tested using the game's existing room management system.

The rooms reference the Zelda 2 movement mechanics already implemented in the player.cpp file (speed values directly from Zelda 2), making them perfect test environments for the game's platforming system.