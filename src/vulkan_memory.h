#pragma once
#include "memory_arena.h"
#include <vulkan/vulkan.h>

// Vulkan memory allocation tracking
struct VulkanAllocation {
    VkDeviceMemory deviceMemory;
    size_t size;
    u32 memoryTypeIndex;
    void* arenaPtr;          // Pointer in our arena for tracking
    bool isActive;
    
    VulkanAllocation() : deviceMemory(VK_NULL_HANDLE), size(0), memoryTypeIndex(0), 
                        arenaPtr(nullptr), isActive(false) {}
};

// Vulkan memory allocator using arena system
namespace VulkanMemory {
    // Initialize Vulkan memory system (call after Vulkan device creation)
    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice);
    
    // Shutdown and cleanup
    void Shutdown();
    
    // Allocate Vulkan device memory using arena allocator
    VkResult AllocateMemory(const VkMemoryRequirements& requirements, 
                           VkMemoryPropertyFlags properties,
                           VkDeviceMemory& outMemory,
                           void** outArenaPtr = nullptr);
    
    // Free Vulkan device memory and associated arena memory
    void FreeMemory(VkDeviceMemory memory);
    
    // Allocate buffer with memory using arena
    VkResult AllocateBuffer(VkDeviceSize size, 
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags memoryProperties,
                           VkBuffer& outBuffer,
                           VkDeviceMemory& outMemory);
    
    // Free buffer and its memory
    void FreeBuffer(VkBuffer buffer, VkDeviceMemory memory);
    
    // Allocate image with memory using arena
    VkResult AllocateImage(const VkImageCreateInfo& imageInfo,
                          VkMemoryPropertyFlags memoryProperties,
                          VkImage& outImage,
                          VkDeviceMemory& outMemory);
    
    // Free image and its memory
    void FreeImage(VkImage image, VkDeviceMemory memory);
    
    // Get memory type index for requirements
    u32 GetMemoryTypeIndex(u32 typeBits, VkMemoryPropertyFlags properties);
    
    // Memory statistics
    void GetVulkanMemoryStats(size_t& totalAllocated, u32& activeAllocations);
    
    // Debug: Print Vulkan memory usage
    void PrintVulkanMemoryStats();
}