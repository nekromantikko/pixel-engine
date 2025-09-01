#ifdef PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <stdlib.h>
#include <cstring>
#include "rendering.h"
#include "software_renderer.h"
#include "debug.h"
#include <SDL_vulkan.h>
#include <cassert>
#include "asset_manager.h"
#include "memory_arena.h"

static constexpr u32 COMMAND_BUFFER_COUNT = 2;
static constexpr u32 SWAPCHAIN_IMAGE_COUNT = 3;
static constexpr u32 EVALUATED_SPRITE_INDEX_BUFFER_SIZE = (MAX_SPRITES_PER_SCANLINE + 1) * SCANLINE_COUNT * sizeof(u32);

struct Quad {
	r32 x, y, w, h;
};
static constexpr Quad DEFAULT_QUAD = { 0, 0, 1, 1 };

struct Image {
	u32 width, height;
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
};

struct Buffer {
	u32 size;
	VkBuffer buffer;
	VkDeviceMemory memory;
};

struct PhysicalDeviceFeatures {
	VkPhysicalDeviceFeatures2 deviceFeatures2;
	VkPhysicalDeviceVulkan11Features vulkan11Features;
	VkPhysicalDeviceVulkan12Features vulkan12Features;

	PhysicalDeviceFeatures()
		: deviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 },
		vulkan11Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES },
		vulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES } {
		deviceFeatures2.pNext = &vulkan11Features;
		vulkan11Features.pNext = &vulkan12Features;
	}
};

struct RenderContext {
	ShaderHandle blitShaderHandle;

	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	// VkPhysicalDeviceFeatures physicalDeviceFeatures;
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;

	VkDevice device;
	u32 primaryQueueFamilyIndex;
	VkQueue primaryQueue;

	VkCommandPool primaryCommandPool;
	VkDescriptorPool descriptorPool;
	VkDescriptorPool imGuiDescriptorPool;

	u32 currentCbIndex;
	VkCommandBuffer primaryCommandBuffers[COMMAND_BUFFER_COUNT];
	VkFence commandBufferFences[COMMAND_BUFFER_COUNT];
	VkSemaphore imageAcquiredSemaphores[COMMAND_BUFFER_COUNT];
	VkSemaphore drawCompleteSemaphores[COMMAND_BUFFER_COUNT];

	VkSwapchainKHR swapchain;
	u32 currentSwaphainIndex;
	VkImage swapchainImages[SWAPCHAIN_IMAGE_COUNT];
	VkImageView swapchainImageViews[SWAPCHAIN_IMAGE_COUNT];
	VkFramebuffer swapchainFramebuffers[SWAPCHAIN_IMAGE_COUNT];

	// General stuff
	VkSampler defaultSampler;

	// Grafix
	VkShaderModule blitShaderModule;
	VkDescriptorSetLayout graphicsDescriptorSetLayout;
	VkDescriptorSet graphicsDescriptorSets[COMMAND_BUFFER_COUNT];
	VkRenderPass renderImagePass;
	VkPipelineLayout blitPipelineLayout;
	VkPipeline blitRawPipeline;
	VkPipeline blitCRTPipeline;

    Buffer framebufferBuffer;
	Image colorImages[COMMAND_BUFFER_COUNT];

	// Settings
	RenderSettings settings;

	// Editor stuff
#ifdef EDITOR
	VkDescriptorSetLayout debugDescriptorSetLayout;

	VkPipelineLayout chrPipelineLayout;
	VkPipeline chrPipeline;
	VkShaderModule chrShaderModule;

	VkPipelineLayout palettePipelineLayout;
	VkPipeline palettePipeline;
	VkShaderModule paletteShaderModule;
#endif
};

static RenderContext g_context;

static void SetObjectName(VkObjectType objectType, uint64_t objectHandle, const char* name) {
#ifndef NDEBUG
	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.pNext = nullptr;
	nameInfo.objectType = objectType;
	nameInfo.objectHandle = objectHandle;
	nameInfo.pObjectName = name;

    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(g_context.instance, "vkSetDebugUtilsObjectNameEXT");
	VkResult err = vkSetDebugUtilsObjectNameEXT(g_context.device, &nameInfo);
	if (err != VK_SUCCESS) {
		DEBUG_WARN("Failed to set object name: %s\n", string_VkResult(err));
	}
#endif
}

static void CreateVulkanInstance() {
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.pEngineName = "";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	const char* debugLayerName = "VK_LAYER_KHRONOS_validation";

	const char* extensionNames[3]{};
	u32 extensionCount = 0;
	extensionNames[extensionCount++] = "VK_KHR_surface";
#ifdef VK_USE_PLATFORM_WIN32_KHR
	extensionNames[extensionCount++] = "VK_KHR_win32_surface";
#endif
#ifndef NDEBUG
	extensionNames[extensionCount++] = "VK_EXT_debug_utils";
#endif

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = &appInfo;
#ifndef NDEBUG
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledLayerNames = &debugLayerName;
#else
    createInfo.enabledLayerCount = 0;
#endif
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensionNames;

	VkResult err = vkCreateInstance(&createInfo, nullptr, &g_context.instance);
	if (err != VK_SUCCESS) {
		DEBUG_FATAL("Failed to create instance: %s\n", string_VkResult(err));
	}
}

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, u32& outQueueFamilyIndex, PhysicalDeviceFeatures& outDeviceFeatures) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	vkGetPhysicalDeviceFeatures2(physicalDevice, &outDeviceFeatures.deviceFeatures2);

	if (!outDeviceFeatures.vulkan12Features.shaderInt8 ||
		!outDeviceFeatures.vulkan12Features.storageBuffer8BitAccess ||
		!outDeviceFeatures.deviceFeatures2.features.shaderInt16 ||
		!outDeviceFeatures.vulkan11Features.storageBuffer16BitAccess ||
		!outDeviceFeatures.deviceFeatures2.features.shaderInt64) {
		return false;
	}

	u32 extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	ArenaMarker scratchMarker = ArenaAllocator::GetMarker(ARENA_SCRATCH);
	VkExtensionProperties* availableExtensions = ArenaAllocator::PushArray<VkExtensionProperties>(ARENA_SCRATCH, extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions);

	bool hasSwapchainSupport = false;

	for (u32 i = 0; i < extensionCount; i++) {
		DEBUG_LOG("%s\n", availableExtensions[i].extensionName);
		if (strcmp(availableExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
			hasSwapchainSupport = true;
			break;
		}
	}

	ArenaAllocator::PopToMarker(ARENA_SCRATCH, scratchMarker);

	if (!hasSwapchainSupport) {
		return false;
	}

	u32 surfaceFormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
	VkSurfaceFormatKHR* availableFormats = ArenaAllocator::PushArray<VkSurfaceFormatKHR>(ARENA_SCRATCH, surfaceFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableFormats);

	u32 presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	VkPresentModeKHR* availablePresentModes = ArenaAllocator::PushArray<VkPresentModeKHR>(ARENA_SCRATCH, presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, availablePresentModes);

	ArenaAllocator::PopToMarker(ARENA_SCRATCH, scratchMarker);

	if (surfaceFormatCount == 0 || presentModeCount == 0) {
		return false;
	}

	u32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	VkQueueFamilyProperties* queueFamilies = ArenaAllocator::PushArray<VkQueueFamilyProperties>(ARENA_SCRATCH, queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

	bool queueFamilyFound = false;
	u32 foundIndex = 0;

	for (u32 i = 0; i < queueFamilyCount; i++) {
		VkQueueFamilyProperties queueFamily = queueFamilies[i];

		if (queueFamily.queueCount == 0) {
			continue;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

		if (!presentSupport) {
			continue;
		}

		// For now, to keep things simple I want to use one queue for everything, so the family needs to support all of these:
		if ((queueFamilies[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))) {
			queueFamilyFound = true;
			foundIndex = i;
			break;
		}
	}

	ArenaAllocator::PopToMarker(ARENA_SCRATCH, scratchMarker);

	if (queueFamilyFound) {
		outQueueFamilyIndex = foundIndex;
		return true;
	}
	return false;
}

static void GetSuitablePhysicalDevice(PhysicalDeviceFeatures& outDeviceFeatures) {
	u32 physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(g_context.instance, &physicalDeviceCount, nullptr);
	if (physicalDeviceCount == 0) {
		DEBUG_ERROR("No devices found for some reason!\n");
	}
	ArenaMarker scratchMarker = ArenaAllocator::GetMarker(ARENA_SCRATCH);
	VkPhysicalDevice* availableDevices = ArenaAllocator::PushArray<VkPhysicalDevice>(ARENA_SCRATCH, physicalDeviceCount);
	vkEnumeratePhysicalDevices(g_context.instance, &physicalDeviceCount, availableDevices);

	bool physicalDeviceFound = false;
	VkPhysicalDevice foundDevice = VK_NULL_HANDLE;
	u32 foundQueueFamilyIndex = 0;

	for (u32 i = 0; i < physicalDeviceCount; i++) {
		VkPhysicalDevice physicalDevice = availableDevices[i];
		u32 queueFamilyIndex;
		if (IsPhysicalDeviceSuitable(physicalDevice, g_context.surface, queueFamilyIndex, outDeviceFeatures)) {
			physicalDeviceFound = true;
			foundDevice = physicalDevice;
			foundQueueFamilyIndex = queueFamilyIndex;
		}
	}

	if (!physicalDeviceFound) {
		DEBUG_ERROR("No suitable physical device found!\n");
	}
	g_context.primaryQueueFamilyIndex = foundQueueFamilyIndex;
	g_context.physicalDevice = foundDevice;

	ArenaAllocator::PopToMarker(ARENA_SCRATCH, scratchMarker);
}

static void CreateDevice(PhysicalDeviceFeatures& deviceFeatures) {
	float queuePriority = 1.0f;

	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.pNext = nullptr;
	queueCreateInfo.flags = 0;
	queueCreateInfo.queueFamilyIndex = g_context.primaryQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	const char* swapchainExtensionName = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = &deviceFeatures.deviceFeatures2;
	createInfo.flags = 0;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.pEnabledFeatures = nullptr;
	createInfo.enabledExtensionCount = 1;
	createInfo.ppEnabledExtensionNames = &swapchainExtensionName;

	VkResult err = vkCreateDevice(g_context.physicalDevice, &createInfo, nullptr, &g_context.device);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create logical device!\n");
	}
}

// TODO: Absorb into other function
static void CreateRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderImagePassInfo{};
	renderImagePassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderImagePassInfo.attachmentCount = 1;
	renderImagePassInfo.pAttachments = &colorAttachment;
	renderImagePassInfo.subpassCount = 1;
	renderImagePassInfo.pSubpasses = &subpass;
	renderImagePassInfo.dependencyCount = 1;
	renderImagePassInfo.pDependencies = &dependency;

	VkResult err = vkCreateRenderPass(g_context.device, &renderImagePassInfo, nullptr, &g_context.renderImagePass);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("failed to create render pass!");
	}
}

static void CreateSwapchain() {
	if (SWAPCHAIN_IMAGE_COUNT > g_context.surfaceCapabilities.maxImageCount || SWAPCHAIN_IMAGE_COUNT < g_context.surfaceCapabilities.minImageCount) {
		DEBUG_ERROR("Image count not supported!\n");
	}

	if (g_context.renderImagePass == VK_NULL_HANDLE) {
		DEBUG_ERROR("Invalid render pass!\n");
	}

	VkResult err;

	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = g_context.surface;
	swapchainCreateInfo.minImageCount = SWAPCHAIN_IMAGE_COUNT;
	swapchainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
	swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainCreateInfo.imageExtent = g_context.surfaceCapabilities.currentExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // As long as we're using a single queue
	swapchainCreateInfo.preTransform = g_context.surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	err = vkCreateSwapchainKHR(g_context.device, &swapchainCreateInfo, nullptr, &g_context.swapchain);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create swapchain!\n");
	}

	u32 imageCount = 0;
	vkGetSwapchainImagesKHR(g_context.device, g_context.swapchain, &imageCount, nullptr);
	if (imageCount != SWAPCHAIN_IMAGE_COUNT) {
		DEBUG_ERROR("Something very weird happened\n");
	}
	vkGetSwapchainImagesKHR(g_context.device, g_context.swapchain, &imageCount, g_context.swapchainImages);

	// Create image views and framebuffers
	for (u32 i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = g_context.swapchainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		err = vkCreateImageView(g_context.device, &imageViewCreateInfo, nullptr, &g_context.swapchainImageViews[i]);
		if (err != VK_SUCCESS) {
			DEBUG_ERROR("Failed to create image view!\n");
		}

		VkImageView attachments[] = {
			g_context.swapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = g_context.renderImagePass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = g_context.surfaceCapabilities.currentExtent.width;
		framebufferInfo.height = g_context.surfaceCapabilities.currentExtent.height;
		framebufferInfo.layers = 1;

		err = vkCreateFramebuffer(g_context.device, &framebufferInfo, nullptr, &g_context.swapchainFramebuffers[i]);
		if (err != VK_SUCCESS) {
			DEBUG_ERROR("Failed to create framebuffer!\n");
		}
	}
}

static void FreeSwapchain() {
	for (u32 i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
		vkDestroyFramebuffer(g_context.device, g_context.swapchainFramebuffers[i], nullptr);
		vkDestroyImageView(g_context.device, g_context.swapchainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(g_context.device, g_context.swapchain, nullptr);
}

static VkShaderModule CreateShaderModule(VkDevice device, u8* code, const u32 size) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = size;
	createInfo.pCode = (const u32*)code;

	VkShaderModule module;
	VkResult err = vkCreateShaderModule(device, &createInfo, nullptr, &module);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create shader module!\n");
	}
	return module;
}

static VkShaderModule CreateShaderModule(ShaderHandle handle) {
	Shader* pShader = AssetManager::GetAsset(handle);
	if (!pShader) {
		return VK_NULL_HANDLE;
	}
	return CreateShaderModule(g_context.device, pShader->GetSource(), pShader->sourceSize);
}

static void CreateGraphicsPipeline()
{
	g_context.blitShaderModule = CreateShaderModule(g_context.blitShaderHandle);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = g_context.blitShaderModule;
	vertShaderStageInfo.pName = "Vertex";
	VkPipelineShaderStageCreateInfo rawFragShaderStageInfo{};
	rawFragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rawFragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	rawFragShaderStageInfo.module = g_context.blitShaderModule;
	rawFragShaderStageInfo.pName = "FragmentRaw";
	VkPipelineShaderStageCreateInfo CRTFragShaderStageInfo{};
	CRTFragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	CRTFragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	CRTFragShaderStageInfo.module = g_context.blitShaderModule;
	CRTFragShaderStageInfo.pName = "FragmentCRT";

	VkPipelineShaderStageCreateInfo rawShaderStages[] = { vertShaderStageInfo, rawFragShaderStageInfo };
	VkPipelineShaderStageCreateInfo CRTShaderStages[] = { vertShaderStageInfo, CRTFragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = nullptr; // Will be set at render time
	viewportState.scissorCount = 1;
	viewportState.pScissors = nullptr; // Will be set at render time

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Quad);
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo blitPipelineLayoutInfo{};
	blitPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	blitPipelineLayoutInfo.setLayoutCount = 1;
	blitPipelineLayoutInfo.pSetLayouts = &g_context.graphicsDescriptorSetLayout;
	blitPipelineLayoutInfo.pushConstantRangeCount = 1;
	blitPipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(g_context.device, &blitPipelineLayoutInfo, nullptr, &g_context.blitPipelineLayout) != VK_SUCCESS) {
		DEBUG_ERROR("failed to create pipeline layout!");
	}

	// Dynamic viewport and scissor, as the window size might change
	// (Although it shouldn't change very often)
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 2;
	dynamicStateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo rawPipelineInfo{};
	rawPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	rawPipelineInfo.stageCount = 2;
	rawPipelineInfo.pStages = rawShaderStages;
	rawPipelineInfo.pVertexInputState = &vertexInputInfo;
	rawPipelineInfo.pInputAssemblyState = &inputAssembly;
	rawPipelineInfo.pViewportState = &viewportState;
	rawPipelineInfo.pRasterizationState = &rasterizer;
	rawPipelineInfo.pMultisampleState = &multisampling;
	rawPipelineInfo.pDepthStencilState = nullptr; // Optional
	rawPipelineInfo.pColorBlendState = &colorBlending;
	rawPipelineInfo.pDynamicState = &dynamicStateInfo;
	rawPipelineInfo.layout = g_context.blitPipelineLayout;
	rawPipelineInfo.renderPass = g_context.renderImagePass;
	rawPipelineInfo.subpass = 0;
	rawPipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	rawPipelineInfo.basePipelineIndex = -1; // Optional

	VkGraphicsPipelineCreateInfo CRTPipelineInfo = rawPipelineInfo;
	CRTPipelineInfo.pStages = CRTShaderStages;

	VkGraphicsPipelineCreateInfo createInfos[] = { rawPipelineInfo, CRTPipelineInfo };
	VkPipeline pipelinesToCreate[] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

	VkResult err = vkCreateGraphicsPipelines(g_context.device, VK_NULL_HANDLE, 2, createInfos, nullptr, pipelinesToCreate);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("failed to create graphics pipelines!");
	}

	g_context.blitRawPipeline = pipelinesToCreate[0];
	g_context.blitCRTPipeline = pipelinesToCreate[1];
}

static void FreeGraphicsPipeline()
{
	vkDestroyPipeline(g_context.device, g_context.blitRawPipeline, nullptr);
	vkDestroyPipeline(g_context.device, g_context.blitCRTPipeline, nullptr);
	vkDestroyPipelineLayout(g_context.device, g_context.blitPipelineLayout, nullptr);
	vkDestroyRenderPass(g_context.device, g_context.renderImagePass, nullptr);
	vkDestroyShaderModule(g_context.device, g_context.blitShaderModule, nullptr);
	vkDestroyDescriptorSetLayout(g_context.device, g_context.graphicsDescriptorSetLayout, nullptr);
}

static u32 GetDeviceMemoryTypeIndex(u32 typeFilter, VkMemoryPropertyFlags propertyFlags) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(g_context.physicalDevice, &memProperties);

	for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
			return i;
		}
	}
	
	DEBUG_ERROR("Failed to find suitable memory type!\n");
	return UINT32_MAX;
}

static bool AllocateMemory(VkMemoryRequirements requirements, VkMemoryPropertyFlags properties, VkDeviceMemory& outMemory) {
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = requirements.size;
	memAllocInfo.memoryTypeIndex = GetDeviceMemoryTypeIndex(requirements.memoryTypeBits, properties);

	VkResult err = vkAllocateMemory(g_context.device, &memAllocInfo, nullptr, &outMemory);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to allocate memory: %s\n", string_VkResult(err));
		return false;
	}

    return true;
}

static bool AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, Buffer& outBuffer) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.flags = 0;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult err = vkCreateBuffer(g_context.device, &bufferInfo, nullptr, &outBuffer.buffer);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create buffer: %s\n", string_VkResult(err));
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(g_context.device, outBuffer.buffer, &memRequirements);
	DEBUG_LOG("Buffer memory required: %d\n", memRequirements.size);

	if (!AllocateMemory(memRequirements, memProps, outBuffer.memory)) {
        DEBUG_ERROR("Failed to allocate buffer memory!\n");
		return false;
	}

	err = vkBindBufferMemory(g_context.device, outBuffer.buffer, outBuffer.memory, 0);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to bind buffer memory: %s\n", string_VkResult(err));
		return false;
	}

	outBuffer.size = size;
	return true;
}

static VkCommandBuffer GetTemporaryCommandBuffer() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = g_context.primaryCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VkResult err = vkAllocateCommandBuffers(g_context.device, &allocInfo, &commandBuffer);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to allocate command buffer: %s\n", string_VkResult(err));
		return VK_NULL_HANDLE;
	}

	return commandBuffer;
}

static void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
	VkCommandBuffer temp = GetTemporaryCommandBuffer();

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(temp, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(temp, src, dst, 1, &copyRegion);

	vkEndCommandBuffer(temp);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &temp;

	vkQueueSubmit(g_context.primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(g_context.primaryQueue);

	vkFreeCommandBuffers(g_context.device, g_context.primaryCommandPool, 1, &temp);
}

static void FreeBuffer(Buffer buffer) {
	vkDestroyBuffer(g_context.device, buffer.buffer, nullptr);
	if (buffer.memory != VK_NULL_HANDLE) {
		vkFreeMemory(g_context.device, buffer.memory, nullptr);
	}
}

static bool CreateImage(u32 width, u32 height, VkImageType type, VkImageUsageFlags usage, Image& outImage, bool srgb = false) {
	const VkFormat format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = type;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	VkResult result = vkCreateImage(g_context.device, &imageInfo, nullptr, &outImage.image);
    if (result != VK_SUCCESS) {
        DEBUG_ERROR("Failed to create image: %s\n", string_VkResult(result));
        return false;
    }

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(g_context.device, outImage.image, &memRequirements);

	if (!AllocateMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImage.memory)) {
		DEBUG_ERROR("Failed to allocate image memory!\n");
		return false;
	}

	result = vkBindImageMemory(g_context.device, outImage.image, outImage.memory, 0);
	if (result != VK_SUCCESS) {
		DEBUG_ERROR("Failed to bind image memory: %s\n", string_VkResult(result));
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = outImage.image;
	viewInfo.viewType = (VkImageViewType)type; // A bit sus but probably fine
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	result = vkCreateImageView(g_context.device, &viewInfo, nullptr, &outImage.view);
	if (result != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create image view: %s\n", string_VkResult(result));
		return false;
	}

	outImage.width = width;
	outImage.height = height;

    return true;
}

static void FreeImage(Image image) {
	vkDestroyImageView(g_context.device, image.view, nullptr);
	vkDestroyImage(g_context.device, image.image, nullptr);
	vkFreeMemory(g_context.device, image.memory, nullptr);
}

static VkImageMemoryBarrier GetImageBarrier(const Image* pImage, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = pImage->image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0; // TODO
	barrier.dstAccessMask = 0; // TODO

	return barrier;
}

static void BeginDraw() {
	// Wait for drawing to finish if it hasn't
	vkWaitForFences(g_context.device, 1, &g_context.commandBufferFences[g_context.currentCbIndex], VK_TRUE, UINT64_MAX);

	// Get next swapchain image index
	VkResult err = vkAcquireNextImageKHR(g_context.device, g_context.swapchain, UINT64_MAX, g_context.imageAcquiredSemaphores[g_context.currentCbIndex], VK_NULL_HANDLE, &g_context.currentSwaphainIndex);
	if (err != VK_SUCCESS) {
	}

	vkResetFences(g_context.device, 1, &g_context.commandBufferFences[g_context.currentCbIndex]);
	vkResetCommandBuffer(g_context.primaryCommandBuffers[g_context.currentCbIndex], 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(g_context.primaryCommandBuffers[g_context.currentCbIndex], &beginInfo) != VK_SUCCESS) {
		DEBUG_ERROR("failed to begin recording command buffer!");
	}

	// Should be ready to draw now!
}

static void CopySoftwareFramebuffer() {
    VkCommandBuffer cmd = g_context.primaryCommandBuffers[g_context.currentCbIndex];
    auto barrier = GetImageBarrier(&g_context.colorImages[g_context.currentCbIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { SOFTWARE_FRAMEBUFFER_WIDTH, SOFTWARE_FRAMEBUFFER_HEIGHT, 1 };

    vkCmdCopyBufferToImage(cmd, g_context.framebufferBuffer.buffer, g_context.colorImages[g_context.currentCbIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier = GetImageBarrier(&g_context.colorImages[g_context.currentCbIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static void BeginRenderPass() {
	VkCommandBuffer commandBuffer = g_context.primaryCommandBuffers[g_context.currentCbIndex];

	VkFramebuffer framebuffer = g_context.swapchainFramebuffers[g_context.currentSwaphainIndex];
	VkExtent2D extent = g_context.surfaceCapabilities.currentExtent;

	VkRenderPassBeginInfo renderImagePassBeginInfo{};
	renderImagePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderImagePassBeginInfo.renderPass = g_context.renderImagePass;
	renderImagePassBeginInfo.framebuffer = framebuffer;
	renderImagePassBeginInfo.renderArea.offset = { 0, 0 };
	renderImagePassBeginInfo.renderArea.extent = extent;
	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	renderImagePassBeginInfo.clearValueCount = 1;
	renderImagePassBeginInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderImagePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void BlitSoftwareResults(const Quad& quad) {
	VkCommandBuffer commandBuffer = g_context.primaryCommandBuffers[g_context.currentCbIndex];
	VkExtent2D extent = g_context.surfaceCapabilities.currentExtent;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_context.settings.useCRTFilter ? g_context.blitCRTPipeline : g_context.blitRawPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)(extent.width);
	viewport.height = (float)(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_context.blitPipelineLayout, 0, 1, &g_context.graphicsDescriptorSets[g_context.currentCbIndex], 0, nullptr);

	vkCmdPushConstants(commandBuffer, g_context.blitPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Quad), &quad);
	vkCmdDraw(commandBuffer, 4, 1, 0, 0);
}

static void EndRenderPass() {
	VkCommandBuffer commandBuffer = g_context.primaryCommandBuffers[g_context.currentCbIndex];
	vkCmdEndRenderPass(commandBuffer);
}

static void EndDraw() {
	VkCommandBuffer commandBuffer = g_context.primaryCommandBuffers[g_context.currentCbIndex];

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		DEBUG_ERROR("failed to record command buffer!");
	}

	// Submit the above commands
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore waitSemaphores[] = { g_context.imageAcquiredSemaphores[g_context.currentCbIndex] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &g_context.primaryCommandBuffers[g_context.currentCbIndex];
	VkSemaphore signalSemaphores[] = { g_context.drawCompleteSemaphores[g_context.currentCbIndex] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	VkResult err = vkQueueSubmit(g_context.primaryQueue, 1, &submitInfo, g_context.commandBufferFences[g_context.currentCbIndex]);

	// Present to swapchain
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapchains[] = { g_context.swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &g_context.currentSwaphainIndex;
	presentInfo.pResults = nullptr; // Optional

	vkQueuePresentKHR(g_context.primaryQueue, &presentInfo);

	// Advance cb index
	g_context.currentCbIndex = (g_context.currentCbIndex + 1) % COMMAND_BUFFER_COUNT;
	// Advance swapchain index
	g_context.currentSwaphainIndex = (g_context.currentSwaphainIndex + 1) % SWAPCHAIN_IMAGE_COUNT;
}

////////////////////////////////////////////////////////////

void Rendering::Init(SDL_Window* sdlWindow) {
	CreateVulkanInstance();

	g_context.surface = VK_NULL_HANDLE;
	SDL_Vulkan_CreateSurface(sdlWindow, g_context.instance, &g_context.surface);

	g_context.blitShaderHandle = AssetManager::GetAssetHandleFromPath<ShaderHandle>("shaders/blit.slang");

	PhysicalDeviceFeatures deviceFeatures = PhysicalDeviceFeatures();
	GetSuitablePhysicalDevice(deviceFeatures);
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_context.physicalDevice, g_context.surface, &g_context.surfaceCapabilities);
	CreateDevice(deviceFeatures);
	vkGetDeviceQueue(g_context.device, g_context.primaryQueueFamilyIndex, 0, &g_context.primaryQueue);
	CreateRenderPass();
	CreateSwapchain();

	VkDescriptorPoolSize poolSizes[3]{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 100;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 100;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[2].descriptorCount = 100;

	VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.poolSizeCount = 3;
	descriptorPoolInfo.pPoolSizes = poolSizes;
	descriptorPoolInfo.maxSets = 100;

	vkCreateDescriptorPool(g_context.device, &descriptorPoolInfo, nullptr, &g_context.descriptorPool);

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerLayoutBinding;

	vkCreateDescriptorSetLayout(g_context.device, &layoutInfo, nullptr, &g_context.graphicsDescriptorSetLayout);

	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
		descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocInfo.descriptorPool = g_context.descriptorPool;
		descriptorSetAllocInfo.descriptorSetCount = 1;
		descriptorSetAllocInfo.pSetLayouts = &g_context.graphicsDescriptorSetLayout;

		vkAllocateDescriptorSets(g_context.device, &descriptorSetAllocInfo, &g_context.graphicsDescriptorSets[i]);
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = g_context.primaryQueueFamilyIndex;

	if (vkCreateCommandPool(g_context.device, &poolInfo, nullptr, &g_context.primaryCommandPool) != VK_SUCCESS) {
		DEBUG_ERROR("failed to create command pool!");
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = g_context.primaryCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = COMMAND_BUFFER_COUNT;

	if (vkAllocateCommandBuffers(g_context.device, &allocInfo, g_context.primaryCommandBuffers) != VK_SUCCESS) {
		DEBUG_ERROR("failed to allocate command buffers!");
	}

	g_context.currentCbIndex = 0;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		vkCreateSemaphore(g_context.device, &semaphoreInfo, nullptr, &g_context.imageAcquiredSemaphores[i]);
		SetObjectName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)g_context.imageAcquiredSemaphores[i], "Image Acquired Semaphore");

		vkCreateSemaphore(g_context.device, &semaphoreInfo, nullptr, &g_context.drawCompleteSemaphores[i]);
		SetObjectName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)g_context.drawCompleteSemaphores[i], "Draw Complete Semaphore");
        
		vkCreateFence(g_context.device, &fenceInfo, nullptr, &g_context.commandBufferFences[i]);
	}

	CreateGraphicsPipeline();

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	vkCreateSampler(g_context.device, &samplerInfo, nullptr, &g_context.defaultSampler);

	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		if (!CreateImage(SOFTWARE_FRAMEBUFFER_WIDTH, SOFTWARE_FRAMEBUFFER_HEIGHT, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, g_context.colorImages[i], true)) {
			DEBUG_FATAL("Failed to create color image!\n");
		}

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = g_context.colorImages[i].view;
		imageInfo.sampler = g_context.defaultSampler;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = g_context.graphicsDescriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(g_context.device, 1, &descriptorWrite, 0, nullptr);
	}

    AllocateBuffer(SOFTWARE_FRAMEBUFFER_SIZE_PIXELS * sizeof(u32), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, g_context.framebufferBuffer);
    u32* framebufferData = nullptr;
    vkMapMemory(g_context.device, g_context.framebufferBuffer.memory, 0, SOFTWARE_FRAMEBUFFER_SIZE_PIXELS * sizeof(u32), 0, (void**)&framebufferData);
    Software::Init(framebufferData);

	g_context.settings = DEFAULT_RENDER_SETTINGS;
}

void Rendering::Free() {
	vkDeviceWaitIdle(g_context.device);

    Software::Free();

    FreeBuffer(g_context.framebufferBuffer);

	// Free imgui stuff
#ifdef EDITOR
	vkDestroyDescriptorPool(g_context.device, g_context.imGuiDescriptorPool, nullptr);
#endif

	vkDestroyDescriptorPool(g_context.device, g_context.descriptorPool, nullptr);

	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
        FreeImage(g_context.colorImages[i]);
		vkDestroySemaphore(g_context.device, g_context.imageAcquiredSemaphores[i], nullptr);
		vkDestroySemaphore(g_context.device, g_context.drawCompleteSemaphores[i], nullptr);
		vkDestroyFence(g_context.device, g_context.commandBufferFences[i], nullptr);
	}
	vkFreeCommandBuffers(g_context.device, g_context.primaryCommandPool, COMMAND_BUFFER_COUNT, g_context.primaryCommandBuffers);
	vkDestroyCommandPool(g_context.device, g_context.primaryCommandPool, nullptr);

	FreeSwapchain();

	FreeGraphicsPipeline();
    vkDestroySampler(g_context.device, g_context.defaultSampler, nullptr);

	vkDestroyDevice(g_context.device, nullptr);
	vkDestroySurfaceKHR(g_context.instance, g_context.surface, nullptr);
	vkDestroyInstance(g_context.instance, nullptr);
}

//////////////////////////////////////////////////////

void Rendering::BeginFrame() {
    Software::DrawFrame();
	BeginDraw();
    CopySoftwareFramebuffer();
}

void Rendering::BeginRenderPass() {
	::BeginRenderPass();
	BlitSoftwareResults(DEFAULT_QUAD);
}

void Rendering::EndFrame() {
	EndRenderPass();
	EndDraw();
}

void Rendering::WaitForAllCommands() {
	vkWaitForFences(g_context.device, COMMAND_BUFFER_COUNT, g_context.commandBufferFences, VK_TRUE, UINT64_MAX);
}

void Rendering::ResizeSurface(u32 width, u32 height) {
	// Wait for all commands to execute first
	vkWaitForFences(g_context.device, COMMAND_BUFFER_COUNT, g_context.commandBufferFences, VK_TRUE, UINT64_MAX);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_context.physicalDevice, g_context.surface, &g_context.surfaceCapabilities);
	FreeSwapchain();
	CreateSwapchain();
}

//////////////////////////////////////////////////////

RenderSettings* Rendering::GetSettings() {
	return &g_context.settings;
}

//////////////////////////////////////////////////////
#ifdef EDITOR
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

static void CreateChrPipeline() {
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(u32) * 2;

	VkPipelineLayoutCreateInfo chrPipelineLayoutInfo{};
	chrPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	chrPipelineLayoutInfo.setLayoutCount = 1;
	chrPipelineLayoutInfo.pSetLayouts = &g_context.debugDescriptorSetLayout;
	chrPipelineLayoutInfo.pushConstantRangeCount = 1;
	chrPipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	vkCreatePipelineLayout(g_context.device, &chrPipelineLayoutInfo, nullptr, &g_context.chrPipelineLayout);

	VkPipelineShaderStageCreateInfo chrShaderStageInfo{};
	chrShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	chrShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	chrShaderStageInfo.module = g_context.softwareShaderModule;
	chrShaderStageInfo.pName = "EditorBlitChr";

	VkComputePipelineCreateInfo chrCreateInfo{};
	chrCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	chrCreateInfo.flags = 0;
	chrCreateInfo.stage = chrShaderStageInfo;
	chrCreateInfo.layout = g_context.chrPipelineLayout;

	vkCreateComputePipelines(g_context.device, VK_NULL_HANDLE, 1, &chrCreateInfo, nullptr, &g_context.chrPipeline);
}

static void CreatePalettePipeline() {
	VkPipelineLayoutCreateInfo chrPipelineLayoutInfo{};
	chrPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	chrPipelineLayoutInfo.setLayoutCount = 1;
	chrPipelineLayoutInfo.pSetLayouts = &g_context.debugDescriptorSetLayout;

	VkResult err = vkCreatePipelineLayout(g_context.device, &chrPipelineLayoutInfo, nullptr, &g_context.palettePipelineLayout);

	VkPipelineShaderStageCreateInfo chrShaderStageInfo{};
	chrShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	chrShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	chrShaderStageInfo.module = g_context.softwareShaderModule;
	chrShaderStageInfo.pName = "EditorBlitPalette";

	VkComputePipelineCreateInfo chrCreateInfo{};
	chrCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	chrCreateInfo.flags = 0;
	chrCreateInfo.stage = chrShaderStageInfo;
	chrCreateInfo.layout = g_context.palettePipelineLayout;

	err = vkCreateComputePipelines(g_context.device, VK_NULL_HANDLE, 1, &chrCreateInfo, nullptr, &g_context.palettePipeline);
}

static void InitGlobalEditorData() {
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding storageLayoutBinding{};
	storageLayoutBinding.binding = 0;
	storageLayoutBinding.descriptorCount = 1;
	storageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageLayoutBinding.pImmutableSamplers = nullptr;
	storageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding paletteLayoutBinding{};
	paletteLayoutBinding.binding = 1;
	paletteLayoutBinding.descriptorCount = 1;
	paletteLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	paletteLayoutBinding.pImmutableSamplers = nullptr;
	paletteLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding chrLayoutBinding{};
	chrLayoutBinding.binding = 2;
	chrLayoutBinding.descriptorCount = 1;
	chrLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	chrLayoutBinding.pImmutableSamplers = nullptr;
	chrLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding palTableBinding{};
	palTableBinding.binding = 3;
	palTableBinding.descriptorCount = 1;
	palTableBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	palTableBinding.pImmutableSamplers = nullptr;
	palTableBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding bindings[] = { storageLayoutBinding, paletteLayoutBinding, chrLayoutBinding, palTableBinding };
	VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
	computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	computeLayoutInfo.bindingCount = 4;
	computeLayoutInfo.pBindings = bindings;

	vkCreateDescriptorSetLayout(g_context.device, &computeLayoutInfo, nullptr, &g_context.debugDescriptorSetLayout);

	// TODO: Should these be destroyed?
	CreateChrPipeline();
	CreatePalettePipeline();
}

static void FreeGlobalEditorData() {
	vkDestroyPipeline(g_context.device, g_context.chrPipeline, nullptr);
	vkDestroyPipelineLayout(g_context.device, g_context.chrPipelineLayout, nullptr);
	vkDestroyShaderModule(g_context.device, g_context.chrShaderModule, nullptr);

	vkDestroyPipeline(g_context.device, g_context.palettePipeline, nullptr);
	vkDestroyPipelineLayout(g_context.device, g_context.palettePipelineLayout, nullptr);
	vkDestroyShaderModule(g_context.device, g_context.paletteShaderModule, nullptr);

	vkDestroyDescriptorSetLayout(g_context.device, g_context.debugDescriptorSetLayout, nullptr);
}

void Rendering::InitEditor(SDL_Window* pWindow) {
	// Likely overkill pool sizes
	VkDescriptorPoolSize poolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = 11;
	poolInfo.pPoolSizes = poolSizes;

	vkCreateDescriptorPool(g_context.device, &poolInfo, nullptr, &g_context.imGuiDescriptorPool);

	ImGui_ImplVulkan_InitInfo vulkanInitInfo{};
	vulkanInitInfo.Instance = g_context.instance;
	vulkanInitInfo.PhysicalDevice = g_context.physicalDevice;
	vulkanInitInfo.Device = g_context.device;
	vulkanInitInfo.QueueFamily = g_context.primaryQueueFamilyIndex;
	vulkanInitInfo.Queue = g_context.primaryQueue;
	vulkanInitInfo.DescriptorPool = g_context.imGuiDescriptorPool;
	vulkanInitInfo.RenderPass = g_context.renderImagePass;
	vulkanInitInfo.MinImageCount = SWAPCHAIN_IMAGE_COUNT;
	vulkanInitInfo.ImageCount = SWAPCHAIN_IMAGE_COUNT;
	vulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplSDL2_InitForVulkan(pWindow);
	ImGui_ImplVulkan_Init(&vulkanInitInfo);
	ImGui_ImplVulkan_CreateFontsTexture();

	InitGlobalEditorData();
}
void Rendering::BeginEditorFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
}
void Rendering::ShutdownEditor() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();

	FreeGlobalEditorData();
}

struct EditorRenderBuffer {
	Buffer buffer;
};

struct EditorRenderTexture {
	u32 usage;
	Image image;
	VkDescriptorSet srcSet;
	VkDescriptorSet dstSet;

	u32 chrSheetOffset;
	u32 chrPaletteOffset;
};

size_t Rendering::GetEditorBufferSize() {
	return sizeof(EditorRenderBuffer);
}

bool Rendering::InitEditorBuffer(EditorRenderBuffer* pBuffer, size_t size, const void* data) {
	if (!pBuffer) {
		DEBUG_ERROR("EditorRenderBuffer pointer is null!");
		return false;
	}

	AllocateBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pBuffer->buffer);
	if (data) {
		UpdateEditorBuffer(pBuffer, data);
	}

	return true;
}
bool Rendering::UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data) {
	if (!pBuffer || !data) {
		return false;
	}

	vkWaitForFences(g_context.device, COMMAND_BUFFER_COUNT, g_context.commandBufferFences, VK_TRUE, UINT64_MAX);
	// TODO: Error check
	void* mappedData;
	vkMapMemory(g_context.device, pBuffer->buffer.memory, 0, pBuffer->buffer.size, 0, &mappedData);
	memcpy(mappedData, data, pBuffer->buffer.size);
	vkUnmapMemory(g_context.device, pBuffer->buffer.memory);

	return true;
}
void Rendering::FreeEditorBuffer(EditorRenderBuffer* pBuffer) {
	if (!pBuffer) {
		return;
	}

	vkWaitForFences(g_context.device, COMMAND_BUFFER_COUNT, g_context.commandBufferFences, VK_TRUE, UINT64_MAX);
	FreeBuffer(pBuffer->buffer);
}

static void UpdateEditorSrcDescriptorSet(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr) {
	VkDescriptorImageInfo outBufferInfo{};
	outBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outBufferInfo.imageView = pTexture->image.view;
	outBufferInfo.sampler = g_context.defaultSampler;

	VkDescriptorImageInfo paletteBufferInfo{};
	paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	paletteBufferInfo.imageView = g_context.paletteImage.view;
	paletteBufferInfo.sampler = g_context.paletteSampler;

	VkDescriptorBufferInfo chrBufferInfo{};
	chrBufferInfo.buffer = g_context.computeBufferDevice[g_context.currentCbIndex].buffer;
	chrBufferInfo.offset = g_context.chrOffset;
	chrBufferInfo.range = g_context.chrSize;

	VkDescriptorBufferInfo palTableInfo{};
	palTableInfo.buffer = g_context.computeBufferDevice[g_context.currentCbIndex].buffer;
	palTableInfo.offset = g_context.paletteTableOffset;
	palTableInfo.range = g_context.paletteTableSize;

	if (pChrBuffer) {
		chrBufferInfo.buffer = pChrBuffer->buffer.buffer;
		chrBufferInfo.offset = 0;
		chrBufferInfo.range = pChrBuffer->buffer.size;
	}

	if (pPaletteBuffer) {
		palTableInfo.buffer = pPaletteBuffer->buffer.buffer;
		palTableInfo.offset = 0;
		palTableInfo.range = pPaletteBuffer->buffer.size;
	}

	VkWriteDescriptorSet descriptorWrite[4]{};
	descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[0].dstSet = pTexture->srcSet;
	descriptorWrite[0].dstBinding = 0;
	descriptorWrite[0].dstArrayElement = 0;
	descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite[0].descriptorCount = 1;
	descriptorWrite[0].pImageInfo = &outBufferInfo;

	descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[1].dstSet = pTexture->srcSet;
	descriptorWrite[1].dstBinding = 1;
	descriptorWrite[1].dstArrayElement = 0;
	descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite[1].descriptorCount = 1;
	descriptorWrite[1].pImageInfo = &paletteBufferInfo;

	descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[2].dstSet = pTexture->srcSet;
	descriptorWrite[2].dstBinding = 2;
	descriptorWrite[2].dstArrayElement = 0;
	descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[2].descriptorCount = 1;
	descriptorWrite[2].pBufferInfo = &chrBufferInfo;

	descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[3].dstSet = pTexture->srcSet;
	descriptorWrite[3].dstBinding = 3;
	descriptorWrite[3].dstArrayElement = 0;
	descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[3].descriptorCount = 1;
	descriptorWrite[3].pBufferInfo = &palTableInfo;

	vkUpdateDescriptorSets(g_context.device, 4, descriptorWrite, 0, nullptr);
}

static void BlitEditorColorsTexture(const EditorRenderTexture* pTexture) {
	if (!pTexture) {
		return;
	}

	VkCommandBuffer temp = GetTemporaryCommandBuffer();

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(temp, &beginInfo);

	VkImageMemoryBarrier barrier = GetImageBarrier(&pTexture->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdPipelineBarrier(
		temp,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	const s32 rowSize = pTexture->image.width;
	const s32 rowCount = COLOR_COUNT / rowSize;
	const s32 texSize = pTexture->image.width * pTexture->image.height;
	const s32 emptyCount = texSize > COLOR_COUNT ? texSize - COLOR_COUNT : 0;
	VkImageBlit regions[COLOR_COUNT]{};
	for (s32 i = 0; i < rowCount; i++) {
		VkImageBlit& region = regions[i];
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0] = { i * rowSize, 0, 0 };
		region.srcOffsets[1] = { i == rowCount - 1 ? s32(COLOR_COUNT) : (i + 1) * rowSize, 1, 1 };
		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.mipLevel = 0;
		region.dstSubresource.baseArrayLayer = 0;
		region.dstSubresource.layerCount = 1;
		region.dstOffsets[0] = { 0, i, 0 };
		region.dstOffsets[1] = { rowSize, i + 1, 1 };
	}

	vkCmdBlitImage(temp, g_context.paletteImage.image, VK_IMAGE_LAYOUT_GENERAL, pTexture->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, rowCount, regions, VK_FILTER_NEAREST);

	barrier = GetImageBarrier(&pTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkCmdPipelineBarrier(
		temp,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	vkEndCommandBuffer(temp);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &temp;

	vkQueueSubmit(g_context.primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(g_context.primaryQueue);

	vkFreeCommandBuffers(g_context.device, g_context.primaryCommandPool, 1, &temp);
}

size_t Rendering::GetEditorTextureSize() {
	return sizeof(EditorRenderTexture);
}

bool Rendering::InitEditorTexture(EditorRenderTexture* pTexture, u32 width, u32 height, u32 usage, u32 chrSheetOffset, u32 chrPaletteOffset, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
	if (!pTexture) {
		DEBUG_ERROR("EditorRenderTexture pointer is null!\n");
		return false;
	}

	pTexture->usage = usage;
	pTexture->chrSheetOffset = chrSheetOffset;
	pTexture->chrPaletteOffset = chrPaletteOffset;

	CreateImage(width, height, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, pTexture->image);
	pTexture->dstSet = ImGui_ImplVulkan_AddTexture(g_context.defaultSampler, pTexture->image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	if (usage == EDITOR_TEXTURE_USAGE_COLOR) {
		BlitEditorColorsTexture(pTexture);
	}
	else {
		VkDescriptorSetAllocateInfo chrDescriptorSetAllocInfo{};
		chrDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		chrDescriptorSetAllocInfo.descriptorPool = g_context.descriptorPool;
		chrDescriptorSetAllocInfo.descriptorSetCount = 1;
		chrDescriptorSetAllocInfo.pSetLayouts = &g_context.debugDescriptorSetLayout;

		vkAllocateDescriptorSets(g_context.device, &chrDescriptorSetAllocInfo, &pTexture->srcSet);
		UpdateEditorSrcDescriptorSet(pTexture, pChrBuffer, pPaletteBuffer);
	}

	return true;
}
bool Rendering::UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
	if (!pTexture) {
		return false;
	}

	if (pTexture->usage == EDITOR_TEXTURE_USAGE_COLOR) {
		DEBUG_ERROR("Cannot update color texture!\n");
		return false;
	}

	vkWaitForFences(g_context.device, COMMAND_BUFFER_COUNT, g_context.commandBufferFences, VK_TRUE, UINT64_MAX);
	UpdateEditorSrcDescriptorSet(pTexture, pChrBuffer, pPaletteBuffer);

	return true;
}
void* Rendering::GetEditorTextureData(const EditorRenderTexture* pTexture) {
	if (!pTexture) {
		return nullptr;
	}

	return (void*)pTexture->dstSet;
}
void Rendering::FreeEditorTexture(EditorRenderTexture* pTexture) {
	if (!pTexture) {
		return;
	}

	vkWaitForFences(g_context.device, COMMAND_BUFFER_COUNT, g_context.commandBufferFences, VK_TRUE, UINT64_MAX);
	ImGui_ImplVulkan_RemoveTexture(pTexture->dstSet);
	FreeImage(pTexture->image);
	// NOTE: Requires VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to free individual descriptor sets
	//vkFreeDescriptorSets(g_context.device, g_context.descriptorPool, 1, &pTexture->srcSet);
	
	// NOTE: Backing memory is from the permanent arena, so we don't free it here
}

void Rendering::RenderEditorTexture(const EditorRenderTexture* pTexture) {
	if (!pTexture) {
		return;
	}

	if (pTexture->usage == EDITOR_TEXTURE_USAGE_COLOR) {
		DEBUG_ERROR("Cannot render color texture!\n");
		return;
	}

	VkCommandBuffer cmd = g_context.primaryCommandBuffers[g_context.currentCbIndex];

	VkImageMemoryBarrier barrier = GetImageBarrier(&pTexture->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	if (pTexture->usage == EDITOR_TEXTURE_USAGE_CHR) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_context.chrPipelineLayout, 0, 1, &pTexture->srcSet, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_context.chrPipeline);

		vkCmdPushConstants(cmd, g_context.chrPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &pTexture->chrSheetOffset);
		vkCmdPushConstants(cmd, g_context.chrPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(u32), sizeof(u32), &pTexture->chrPaletteOffset);

		vkCmdDispatch(cmd, pTexture->image.width / TILE_DIM_PIXELS, pTexture->image.height / TILE_DIM_PIXELS, 1);
	}
	else if (pTexture->usage == EDITOR_TEXTURE_USAGE_PALETTE) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_context.palettePipelineLayout, 0, 1, &pTexture->srcSet, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_context.palettePipeline);
		vkCmdDispatch(cmd, pTexture->image.width / 8, 1, 1);
	}

	barrier = GetImageBarrier(&pTexture->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

void Rendering::RenderEditor() {
	VkCommandBuffer commandBuffer = g_context.primaryCommandBuffers[g_context.currentCbIndex];

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData != nullptr) {
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}
}
#endif