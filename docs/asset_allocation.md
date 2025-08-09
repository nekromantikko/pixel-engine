# AssetArchive Allocation System

The AssetArchive class now supports configurable memory allocation strategies to optimize for different usage contexts:

## Allocation Strategies

### 1. Arena Allocation (Engine Runtime)
- **Usage**: Game engine runtime when arena allocator is initialized
- **Benefits**: 
  - Eliminates heap fragmentation
  - Better cache locality for sequential asset access
  - Faster cleanup (reset arena vs individual frees)
  - No malloc/free overhead for frequent allocations
- **Trade-offs**: 
  - Cannot free individual assets (memory stays until arena reset)
  - Realloc operations may require copying data

### 2. Malloc Allocation (Asset Packer & Fallback)
- **Usage**: Asset packer tool and when arena system unavailable
- **Benefits**:
  - Standard memory management behavior
  - Can free individual allocations
  - Compatible with existing tools
- **Trade-offs**:
  - Potential heap fragmentation
  - Individual malloc/free overhead

## Usage

### Automatic Selection (Recommended)
```cpp
AssetArchive archive; // Uses GetDefaultAllocator()
```
The archive automatically selects:
- Arena allocation if `ArenaAllocator::IsInitialized()` returns true
- Malloc allocation otherwise

### Explicit Selection
```cpp
// Force malloc allocation (asset packer)
AssetAllocator mallocAlloc = AssetAllocators::GetMallocAllocator();
AssetArchive archive(mallocAlloc);

// Force arena allocation (when you know arenas are available)  
AssetAllocator arenaAlloc = AssetAllocators::GetArenaAllocator();
AssetArchive archive(arenaAlloc);
```

## Implementation Details

The system uses function pointer callbacks in `AssetAllocator` struct:
- `Alloc(size_t size, void* userData)` - Allocate memory
- `Realloc(void* ptr, size_t oldSize, size_t newSize, void* userData)` - Resize allocation
- `Free(void* ptr, void* userData)` - Free memory

Arena realloc is optimized to avoid copying when possible, but may allocate new blocks for growing operations to maintain arena allocation patterns.

## Performance

Both allocation strategies have similar performance for typical asset loading patterns. Arena allocation provides better long-term performance characteristics due to reduced fragmentation and improved cache locality.