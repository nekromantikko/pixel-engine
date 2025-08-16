#!/bin/bash
# Simple verification script for destructible blocks implementation

echo "=== Destructible Blocks Implementation Verification ==="
echo

# Check if TILE_DESTRUCTIBLE is defined
echo "1. Checking TILE_DESTRUCTIBLE enum definition:"
grep -n "TILE_DESTRUCTIBLE" src/core_types.h || echo "ERROR: TILE_DESTRUCTIBLE not found"
echo

# Check if METATILE_TYPE_NAMES includes destructible
echo "2. Checking METATILE_TYPE_NAMES array:"
grep -A1 -B1 "Destructible" src/core_types.h || echo "ERROR: Destructible name not found"
echo

# Check effect type definition
echo "3. Checking EFFECT_TYPE_TILE_DEBRIS definition:"
grep -n "EFFECT_TYPE_TILE_DEBRIS" src/actor_data.h || echo "ERROR: EFFECT_TYPE_TILE_DEBRIS not found"
echo

# Check collision system updates
echo "4. Checking collision system handles destructible tiles:"
grep -n "TILE_DESTRUCTIBLE" src/collision.cpp | head -2 || echo "ERROR: Collision system not updated"
echo

# Check bullet system updates
echo "5. Checking bullet system handles destructible tiles:"
grep -n "TILE_DESTRUCTIBLE" src/bullet.cpp | head -2 || echo "ERROR: Bullet system not updated"
echo

# Check tile destruction function
echo "6. Checking tile destruction function:"
grep -n "DestroyTileAt" src/game_state.h src/game_state.cpp | head -2 || echo "ERROR: DestroyTileAt function not found"
echo

# Check HitResult has tileCoord
echo "7. Checking HitResult has tileCoord field:"
grep -A2 -B2 "tileCoord" src/core_types.h || echo "ERROR: tileCoord field not found in HitResult"
echo

echo "=== Verification Complete ==="