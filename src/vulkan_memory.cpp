#include "vulkan_memory.h"
#include "debug.h"
#include <cassert>
#include <vector>

namespace VulkanMemory {
    // Vulkan context
    static VkDevice g_device = VK_NULL_HANDLE;
    static VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
    static VkPhysicalDeviceMemoryProperties g_memoryProperties{};
    static bool g_initialized = false;
    
    // Allocation tracking
    static constexpr u32 MAX_VULKAN_ALLOCATIONS = 512;
    static VulkanAllocation g_allocations[MAX_VULKAN_ALLOCATIONS];
    static u32 g_allocationCount = 0;
    static size_t g_totalAllocated = 0;
    
    // Find free allocation slot
    static u32 FindFreeAllocationSlot() {
        for (u32 i = 0; i < MAX_VULKAN_ALLOCATIONS; i++) {
            if (!g_allocations[i].isActive) {
                return i;
            }
        }
        return UINT32_MAX;
    }
    
    // Find allocation by device memory handle
    static u32 FindAllocationByMemory(VkDeviceMemory memory) {
        for (u32 i = 0; i < MAX_VULKAN_ALLOCATIONS; i++) {
            if (g_allocations[i].isActive && g_allocations[i].deviceMemory == memory) {
                return i;
            }
        }
        return UINT32_MAX;
    }
    
    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice) {
        if (g_initialized) {
            DEBUG_WARN("VulkanMemory already initialized!\n");
            return;
        }
        
        g_device = device;
        g_physicalDevice = physicalDevice;
        
        // Get memory properties
        vkGetPhysicalDeviceMemoryProperties(g_physicalDevice, &g_memoryProperties);
        
        // Clear allocation tracking
        for (u32 i = 0; i < MAX_VULKAN_ALLOCATIONS; i++) {
            g_allocations[i] = VulkanAllocation();
        }
        g_allocationCount = 0;
        g_totalAllocated = 0;
        
        g_initialized = true;
        DEBUG_LOG("VulkanMemory system initialized\n");
        
        // Print available memory heaps
        DEBUG_LOG("Vulkan Memory Heaps:\n");
        for (u32 i = 0; i < g_memoryProperties.memoryHeapCount; i++) {
            const VkMemoryHeap& heap = g_memoryProperties.memoryHeaps[i];
            DEBUG_LOG("  Heap %u: %zu MB %s\n", i, heap.size / (1024 * 1024),
                     (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "(Device Local)" : "(Host)");
        }
    }
    
    void Shutdown() {
        if (!g_initialized) {
            return;
        }
        
        DEBUG_LOG("Shutting down VulkanMemory system\n");
        PrintVulkanMemoryStats();
        
        // Free any remaining allocations
        for (u32 i = 0; i < MAX_VULKAN_ALLOCATIONS; i++) {
            if (g_allocations[i].isActive) {
                DEBUG_WARN("Leaked Vulkan memory allocation %u (size: %zu bytes)\n", 
                          i, g_allocations[i].size);
                vkFreeMemory(g_device, g_allocations[i].deviceMemory, nullptr);
                g_allocations[i] = VulkanAllocation();
            }
        }
        
        g_device = VK_NULL_HANDLE;
        g_physicalDevice = VK_NULL_HANDLE;
        g_allocationCount = 0;
        g_totalAllocated = 0;
        g_initialized = false;
        
        DEBUG_LOG("VulkanMemory system shutdown complete\n");
    }
    
    u32 GetMemoryTypeIndex(u32 typeBits, VkMemoryPropertyFlags properties) {
        assert(g_initialized && "VulkanMemory not initialized!");
        
        for (u32 i = 0; i < g_memoryProperties.memoryTypeCount; i++) {
            if ((typeBits & (1 << i)) && 
                (g_memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        
        DEBUG_FATAL("Failed to find suitable memory type!\n");
        return UINT32_MAX;
    }
    
    VkResult AllocateMemory(const VkMemoryRequirements& requirements,
                           VkMemoryPropertyFlags properties,
                           VkDeviceMemory& outMemory,
                           void** outArenaPtr) {
        assert(g_initialized && "VulkanMemory not initialized!");
        
        u32 slot = FindFreeAllocationSlot();
        if (slot == UINT32_MAX) {
            DEBUG_ERROR("Too many Vulkan allocations! Increase MAX_VULKAN_ALLOCATIONS\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        
        // Find memory type
        u32 memoryTypeIndex = GetMemoryTypeIndex(requirements.memoryTypeBits, properties);
        if (memoryTypeIndex == UINT32_MAX) {
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
        
        // Allocate from arena for tracking
        void* arenaPtr = ArenaAllocator::Allocate(ArenaAllocator::ARENA_VULKAN, requirements.size);
        if (!arenaPtr) {
            DEBUG_ERROR("Failed to allocate Vulkan tracking memory from arena\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        
        // Allocate Vulkan device memory
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        
        VkResult result = vkAllocateMemory(g_device, &allocInfo, nullptr, &outMemory);
        if (result != VK_SUCCESS) {
            DEBUG_ERROR("Failed to allocate Vulkan device memory: %d\n", result);
            return result;
        }
        
        // Track allocation
        VulkanAllocation& allocation = g_allocations[slot];
        allocation.deviceMemory = outMemory;
        allocation.size = requirements.size;
        allocation.memoryTypeIndex = memoryTypeIndex;
        allocation.arenaPtr = arenaPtr;
        allocation.isActive = true;
        
        g_allocationCount++;
        g_totalAllocated += requirements.size;
        
        if (outArenaPtr) {
            *outArenaPtr = arenaPtr;
        }
        
        DEBUG_LOG("Allocated Vulkan memory: %zu bytes (slot: %u, total: %zu bytes, count: %u)\n",
                 requirements.size, slot, g_totalAllocated, g_allocationCount);
        
        return VK_SUCCESS;
    }
    
    void FreeMemory(VkDeviceMemory memory) {
        assert(g_initialized && "VulkanMemory not initialized!");
        
        if (memory == VK_NULL_HANDLE) {
            return;
        }
        
        u32 slot = FindAllocationByMemory(memory);
        if (slot == UINT32_MAX) {
            DEBUG_WARN("Attempting to free unknown Vulkan memory!\n");
            vkFreeMemory(g_device, memory, nullptr);
            return;
        }
        
        VulkanAllocation& allocation = g_allocations[slot];
        
        DEBUG_LOG("Freeing Vulkan memory: %zu bytes (slot: %u)\n", allocation.size, slot);
        
        // Free Vulkan memory
        vkFreeMemory(g_device, memory, nullptr);
        
        // Update tracking
        g_allocationCount--;
        g_totalAllocated -= allocation.size;
        
        // Clear allocation
        allocation = VulkanAllocation();
    }
    
    VkResult AllocateBuffer(VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags memoryProperties,
                           VkBuffer& outBuffer,
                           VkDeviceMemory& outMemory) {
        // Create buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkResult result = vkCreateBuffer(g_device, &bufferInfo, nullptr, &outBuffer);
        if (result != VK_SUCCESS) {
            return result;
        }
        
        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(g_device, outBuffer, &memRequirements);
        
        // Allocate memory
        result = AllocateMemory(memRequirements, memoryProperties, outMemory);
        if (result != VK_SUCCESS) {
            vkDestroyBuffer(g_device, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            return result;
        }
        
        // Bind buffer to memory
        result = vkBindBufferMemory(g_device, outBuffer, outMemory, 0);
        if (result != VK_SUCCESS) {
            FreeMemory(outMemory);
            vkDestroyBuffer(g_device, outBuffer, nullptr);
            outBuffer = VK_NULL_HANDLE;
            outMemory = VK_NULL_HANDLE;
            return result;
        }
        
        return VK_SUCCESS;
    }
    
    void FreeBuffer(VkBuffer buffer, VkDeviceMemory memory) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(g_device, buffer, nullptr);
        }
        FreeMemory(memory);
    }
    
    VkResult AllocateImage(const VkImageCreateInfo& imageInfo,
                          VkMemoryPropertyFlags memoryProperties,
                          VkImage& outImage,
                          VkDeviceMemory& outMemory) {
        // Create image
        VkResult result = vkCreateImage(g_device, &imageInfo, nullptr, &outImage);
        if (result != VK_SUCCESS) {
            return result;
        }
        
        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(g_device, outImage, &memRequirements);
        
        // Allocate memory
        result = AllocateMemory(memRequirements, memoryProperties, outMemory);
        if (result != VK_SUCCESS) {
            vkDestroyImage(g_device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            return result;
        }
        
        // Bind image to memory
        result = vkBindImageMemory(g_device, outImage, outMemory, 0);
        if (result != VK_SUCCESS) {
            FreeMemory(outMemory);
            vkDestroyImage(g_device, outImage, nullptr);
            outImage = VK_NULL_HANDLE;
            outMemory = VK_NULL_HANDLE;
            return result;
        }
        
        return VK_SUCCESS;
    }
    
    void FreeImage(VkImage image, VkDeviceMemory memory) {
        if (image != VK_NULL_HANDLE) {
            vkDestroyImage(g_device, image, nullptr);
        }
        FreeMemory(memory);
    }
    
    void GetVulkanMemoryStats(size_t& totalAllocated, u32& activeAllocations) {
        totalAllocated = g_totalAllocated;
        activeAllocations = g_allocationCount;
    }
    
    void PrintVulkanMemoryStats() {
        if (!g_initialized) {
            DEBUG_LOG("VulkanMemory not initialized\n");
            return;
        }
        
        DEBUG_LOG("=== Vulkan Memory Statistics ===\n");
        DEBUG_LOG("Active allocations: %u / %u\n", g_allocationCount, MAX_VULKAN_ALLOCATIONS);
        DEBUG_LOG("Total allocated: %zu bytes (%.2f MB)\n", 
                 g_totalAllocated, r32(g_totalAllocated) / (1024.0f * 1024.0f));
        
        if (g_allocationCount > 0) {
            DEBUG_LOG("Active allocation details:\n");
            for (u32 i = 0; i < MAX_VULKAN_ALLOCATIONS; i++) {
                if (g_allocations[i].isActive) {
                    DEBUG_LOG("  Slot %u: %zu bytes (type: %u)\n", 
                             i, g_allocations[i].size, g_allocations[i].memoryTypeIndex);
                }
            }
        }
        DEBUG_LOG("==============================\n");
    }
}