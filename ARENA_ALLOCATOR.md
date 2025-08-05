# Arena Allocator System Documentation

## Overview
The arena allocator system provides fast, stack-based memory management for the pixel engine. It replaces scattered `malloc/free` calls with centralized, predictable memory allocation patterns.

## Architecture
The system provides three specialized arenas:

### 1. Permanent Arena (64MB)
- **Purpose**: Long-lived allocations that persist for the entire application lifetime
- **Usage**: Game data, assets, renderer state, configuration data
- **Pattern**: Allocate once during initialization, never explicitly freed

### 2. Temporary Arena (16MB) 
- **Purpose**: Short-lived allocations for temporary calculations and operations
- **Usage**: Frame processing, temporary file loading, string building, algorithm scratch space
- **Pattern**: Use markers for stack-style allocation/deallocation, clear at end of operations

### 3. Vulkan Arena (128MB)
- **Purpose**: Tracking and managing Vulkan GPU memory allocations
- **Usage**: Vulkan buffers, images, and device memory (managed via VulkanMemory system)
- **Pattern**: Automatic tracking of GPU memory with debugging support

## Usage Examples

### Basic Allocation
```cpp
// Single object allocation
MyStruct* obj = ArenaAllocator::Allocate<MyStruct>(ArenaAllocator::ARENA_PERMANENT);

// Array allocation  
float* vertices = ArenaAllocator::AllocateArray<float>(ArenaAllocator::ARENA_TEMPORARY, 1000);
```

### Stack-Style Memory Management
```cpp
// Save current position
ArenaMarker marker = ArenaAllocator::GetMarker(ArenaAllocator::ARENA_TEMPORARY);

// Do temporary work
char* tempBuffer = ArenaAllocator::AllocateArray<char>(ArenaAllocator::ARENA_TEMPORARY, 4096);
ProcessData(tempBuffer);

// Restore previous position (frees all allocations after marker)
ArenaAllocator::ResetToMarker(marker);
```

### Vulkan Memory Integration
```cpp
// Vulkan memory is automatically managed through VulkanMemory system
VkBuffer buffer;
VkDeviceMemory memory;
VkResult result = VulkanMemory::AllocateBuffer(
    bufferSize, 
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    buffer, 
    memory
);

// Memory tracking and debugging
VulkanMemory::PrintVulkanMemoryStats();
```

## Memory Usage Guidelines

### DO Use Arena Allocation For:
- Game object data (use permanent arena)
- Temporary processing buffers (use temporary arena with markers)
- Asset loading operations
- String building and manipulation
- Algorithm scratch space

### DON'T Use Arena Allocation For:
- Editor UI code (can use heap allocation for convenience)
- Dynamic containers that frequently resize
- Objects with unpredictable lifetimes

### Frame-Based Pattern
```cpp
void ProcessFrame() {
    // Clear temporary arena at start of frame
    ArenaAllocator::ClearTemporaryArena();
    
    // Use temporary arena for frame processing
    TempData* frameData = ArenaAllocator::AllocateArray<TempData>(
        ArenaAllocator::ARENA_TEMPORARY, frameObjects);
    
    // Process frame...
    
    // Temporary arena automatically cleared next frame
}
```

## Memory Statistics and Debugging
```cpp
// Print overall memory usage
ArenaAllocator::PrintMemoryStats();

// Get specific arena stats
size_t used, total;
float percent;
ArenaAllocator::GetMemoryStats(ArenaAllocator::ARENA_VULKAN, used, total, percent);

// Print Vulkan-specific memory stats
VulkanMemory::PrintVulkanMemoryStats();
```

## Performance Benefits
- **No fragmentation**: Linear allocation eliminates memory fragmentation
- **Cache friendly**: Sequential memory layout improves cache performance  
- **Fast allocation**: O(1) allocation vs O(log n) for general heap
- **Predictable**: No unpredictable malloc/free performance spikes
- **Debugging**: Centralized tracking makes memory leaks easier to find

## Integration with Existing Systems
- **Pool system**: Existing `Pool<T>` templates work unchanged (use stack allocation)
- **Asset system**: Dynamic asset loading still uses heap (appropriate for variable-size data)
- **Editor code**: Still uses heap allocation per coding standards
- **Vulkan memory**: Completely replaced with arena-based tracking system