#ifdef PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <cstring>
#include "rendering.h"
#include "rendering_util.h"
#include "system.h"
#include <SDL_vulkan.h>
#include <cassert>
#include <vector>

#ifdef EDITOR
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#endif

static constexpr u32 COMMAND_BUFFER_COUNT = 2;
static constexpr u32 SWAPCHAIN_IMAGE_COUNT = 3;

struct Quad {
	r32 x, y, w, h;
};
static constexpr Quad DEFAULT_QUAD = { 0, 0, 1, 1 };

struct ScanlineData {
	u32 spriteCount;
	u32 spriteIndices[MAX_SPRITES_PER_SCANLINE];

	s32 scrollX;
	s32 scrollY;
};

struct Image {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
};

struct Buffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
};

struct RenderContext {
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
	VkShaderModule blitVertexShaderModule;
	VkShaderModule blitRawFragmentShaderModule;
	VkShaderModule blitCRTFragmentShaderModule;
	VkDescriptorSetLayout graphicsDescriptorSetLayout;
	VkDescriptorSet graphicsDescriptorSets[COMMAND_BUFFER_COUNT];
	VkRenderPass renderImagePass;
	VkPipelineLayout blitPipelineLayout;
	VkPipeline blitRawPipeline;
	VkPipeline blitCRTPipeline;

	// Compute stuff
	Image paletteImage;

	u32 paletteTableOffset;
	u32 paletteTableSize;
	u32 chrOffset;
	u32 chrSize;
	u32 nametableOffset;
	u32 nametableSize;
	u32 oamOffset;
	u32 oamSize;
	u32 renderStateOffset;
	u32 renderStateSize;
	u32 computeBufferSize;

	void* renderData;
	Buffer computeBufferDevice;
	Buffer computeBufferHost[COMMAND_BUFFER_COUNT];

	Buffer scanlineBuffer;

	Image colorImage;

	VkDescriptorSetLayout computeDescriptorSetLayout;
	VkDescriptorSet computeDescriptorSet;
	VkPipelineLayout softwarePipelineLayout;
	VkPipeline softwarePipeline;
	VkShaderModule softwareShaderModule;
	VkPipelineLayout evaluatePipelineLayout;
	VkPipeline evaluatePipeline;
	VkShaderModule evaluateShaderModule;

	// Settings
	RenderSettings settings;

	// Editor stuff
#ifdef EDITOR
	Image debugChrImage[PALETTE_COUNT];
	VkPipelineLayout chrPipelineLayout;
	VkPipeline chrPipeline;
	VkShaderModule chrShaderModule;
	VkDescriptorSet chrDescriptorSet[PALETTE_COUNT];

	Image debugPaletteImage;
	VkPipelineLayout palettePipelineLayout;
	VkPipeline palettePipeline;
	VkShaderModule paletteShaderModule;
	VkDescriptorSet paletteDescriptorSet;

	bool32 useIntermediateFramebuffer = false;
	Image intermediateImage;
	VkFramebuffer intermediateFramebuffer;
#endif
};

static RenderContext* pContext = nullptr;

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

	const char* extensionNames[2]{};
	u32 extensionCount = 0;
	extensionNames[extensionCount++] = "VK_KHR_surface";
#ifdef VK_USE_PLATFORM_WIN32_KHR
	extensionNames[extensionCount++] = "VK_KHR_win32_surface";
#endif

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledLayerNames = &debugLayerName;
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensionNames;

	VkResult err = vkCreateInstance(&createInfo, nullptr, &pContext->instance);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create instance!\n");
	}
}

static bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, u32& outQueueFamilyIndex) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	u32 extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	VkExtensionProperties* availableExtensions = (VkExtensionProperties*)calloc(extensionCount, sizeof(VkExtensionProperties));
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions);

	bool hasSwapchainSupport = false;

	for (u32 i = 0; i < extensionCount; i++) {
		DEBUG_LOG("%s\n", availableExtensions[i].extensionName);
		if (strcmp(availableExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
			hasSwapchainSupport = true;
			break;
		}
	}

	free(availableExtensions);

	if (!hasSwapchainSupport) {
		return false;
	}

	u32 surfaceFormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
	VkSurfaceFormatKHR* availableFormats = (VkSurfaceFormatKHR*)calloc(surfaceFormatCount, sizeof(VkSurfaceFormatKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableFormats);

	u32 presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	VkPresentModeKHR* availablePresentModes = (VkPresentModeKHR*)calloc(presentModeCount, sizeof(VkPresentModeKHR));
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, availablePresentModes);

	// TODO: Something with these
	free(availableFormats);
	free(availablePresentModes);

	if (surfaceFormatCount == 0 || presentModeCount == 0) {
		return false;
	}

	u32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)calloc(queueFamilyCount, sizeof(VkQueueFamilyProperties));
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

	free(queueFamilies);

	if (queueFamilyFound) {
		outQueueFamilyIndex = foundIndex;
		return true;
	}
	return false;
}

static void GetSuitablePhysicalDevice() {
	u32 physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(pContext->instance, &physicalDeviceCount, nullptr);
	if (physicalDeviceCount == 0) {
		DEBUG_ERROR("No devices found for some reason!\n");
	}
	std::vector<VkPhysicalDevice> availableDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(pContext->instance, &physicalDeviceCount, availableDevices.data());

	bool physicalDeviceFound = false;
	VkPhysicalDevice foundDevice = VK_NULL_HANDLE;
	u32 foundQueueFamilyIndex = 0;

	for (auto& physicalDevice : availableDevices) {
		u32 queueFamilyIndex;
		if (IsPhysicalDeviceSuitable(physicalDevice, pContext->surface, queueFamilyIndex)) {
			physicalDeviceFound = true;
			foundDevice = physicalDevice;
			foundQueueFamilyIndex = queueFamilyIndex;
		}
	}

	if (!physicalDeviceFound) {
		DEBUG_ERROR("No suitable physical device found!\n");
	}
	pContext->primaryQueueFamilyIndex = foundQueueFamilyIndex;
	pContext->physicalDevice = foundDevice;
}

static void CreateDevice() {
	float queuePriority = 1.0f;

	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.pNext = nullptr;
	queueCreateInfo.flags = 0;
	queueCreateInfo.queueFamilyIndex = pContext->primaryQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures{};

	const char* swapchainExtensionName = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = 1;
	createInfo.ppEnabledExtensionNames = &swapchainExtensionName;

	VkResult err = vkCreateDevice(pContext->physicalDevice, &createInfo, nullptr, &pContext->device);
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

	VkResult err = vkCreateRenderPass(pContext->device, &renderImagePassInfo, nullptr, &pContext->renderImagePass);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("failed to create render pass!");
	}
}

static void CreateSwapchain() {
	if (SWAPCHAIN_IMAGE_COUNT > pContext->surfaceCapabilities.maxImageCount || SWAPCHAIN_IMAGE_COUNT < pContext->surfaceCapabilities.minImageCount) {
		DEBUG_ERROR("Image count not supported!\n");
	}

	if (pContext->renderImagePass == VK_NULL_HANDLE) {
		DEBUG_ERROR("Invalid render pass!\n");
	}

	VkResult err;

	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = pContext->surface;
	swapchainCreateInfo.minImageCount = SWAPCHAIN_IMAGE_COUNT;
	swapchainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
	swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainCreateInfo.imageExtent = pContext->surfaceCapabilities.currentExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // As long as we're using a single queue
	swapchainCreateInfo.preTransform = pContext->surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	err = vkCreateSwapchainKHR(pContext->device, &swapchainCreateInfo, nullptr, &pContext->swapchain);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create swapchain!\n");
	}

	u32 imageCount = 0;
	vkGetSwapchainImagesKHR(pContext->device, pContext->swapchain, &imageCount, nullptr);
	if (imageCount != SWAPCHAIN_IMAGE_COUNT) {
		DEBUG_ERROR("Something very weird happened\n");
	}
	vkGetSwapchainImagesKHR(pContext->device, pContext->swapchain, &imageCount, pContext->swapchainImages);

	// Create image views and framebuffers
	for (u32 i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = pContext->swapchainImages[i];
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

		err = vkCreateImageView(pContext->device, &imageViewCreateInfo, nullptr, &pContext->swapchainImageViews[i]);
		if (err != VK_SUCCESS) {
			DEBUG_ERROR("Failed to create image view!\n");
		}

		VkImageView attachments[] = {
			pContext->swapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = pContext->renderImagePass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = pContext->surfaceCapabilities.currentExtent.width;
		framebufferInfo.height = pContext->surfaceCapabilities.currentExtent.height;
		framebufferInfo.layers = 1;

		err = vkCreateFramebuffer(pContext->device, &framebufferInfo, nullptr, &pContext->swapchainFramebuffers[i]);
		if (err != VK_SUCCESS) {
			DEBUG_ERROR("Failed to create framebuffer!\n");
		}
	}
}

static void FreeSwapchain() {
	for (u32 i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
		vkDestroyFramebuffer(pContext->device, pContext->swapchainFramebuffers[i], nullptr);
		vkDestroyImageView(pContext->device, pContext->swapchainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(pContext->device, pContext->swapchain, nullptr);
}

static VkShaderModule CreateShaderModule(VkDevice device, const char* code, const u32 size) {
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

static void CreateGraphicsPipeline()
{
	// TODO: File handling here is bad, move somewhere else
	u32 vertShaderLength;
	char* vertShader = AllocFileBytes("assets/shaders/quad.spv", vertShaderLength);
	u32 rawFragShaderLength;
	char* rawFragShader = AllocFileBytes("assets/shaders/textured_raw.spv", rawFragShaderLength);
	u32 CRTFragShaderLength;
	char* CRTFragShader = AllocFileBytes("assets/shaders/textured_crt.spv", CRTFragShaderLength);

	pContext->blitVertexShaderModule = CreateShaderModule(pContext->device, vertShader, vertShaderLength);
	pContext->blitRawFragmentShaderModule = CreateShaderModule(pContext->device, rawFragShader, rawFragShaderLength);
	pContext->blitCRTFragmentShaderModule = CreateShaderModule(pContext->device, CRTFragShader, CRTFragShaderLength);
	free(vertShader);
	free(rawFragShader);
	free(CRTFragShader);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = pContext->blitVertexShaderModule;
	vertShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo rawFragShaderStageInfo{};
	rawFragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rawFragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	rawFragShaderStageInfo.module = pContext->blitRawFragmentShaderModule;
	rawFragShaderStageInfo.pName = "main";
	VkPipelineShaderStageCreateInfo CRTFragShaderStageInfo{};
	CRTFragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	CRTFragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	CRTFragShaderStageInfo.module = pContext->blitCRTFragmentShaderModule;
	CRTFragShaderStageInfo.pName = "main";

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
	blitPipelineLayoutInfo.pSetLayouts = &pContext->graphicsDescriptorSetLayout;
	blitPipelineLayoutInfo.pushConstantRangeCount = 1;
	blitPipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(pContext->device, &blitPipelineLayoutInfo, nullptr, &pContext->blitPipelineLayout) != VK_SUCCESS) {
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
	rawPipelineInfo.layout = pContext->blitPipelineLayout;
	rawPipelineInfo.renderPass = pContext->renderImagePass;
	rawPipelineInfo.subpass = 0;
	rawPipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	rawPipelineInfo.basePipelineIndex = -1; // Optional

	VkGraphicsPipelineCreateInfo CRTPipelineInfo = rawPipelineInfo;
	CRTPipelineInfo.pStages = CRTShaderStages;

	VkGraphicsPipelineCreateInfo createInfos[] = { rawPipelineInfo, CRTPipelineInfo };
	VkPipeline pipelinesToCreate[] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

	VkResult err = vkCreateGraphicsPipelines(pContext->device, VK_NULL_HANDLE, 2, createInfos, nullptr, pipelinesToCreate);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("failed to create graphics pipelines!");
	}

	pContext->blitRawPipeline = pipelinesToCreate[0];
	pContext->blitCRTPipeline = pipelinesToCreate[1];
}

static void FreeGraphicsPipeline()
{
	vkDestroyPipeline(pContext->device, pContext->blitRawPipeline, nullptr);
	vkDestroyPipeline(pContext->device, pContext->blitCRTPipeline, nullptr);
	vkDestroyPipelineLayout(pContext->device, pContext->blitPipelineLayout, nullptr);
	vkDestroyRenderPass(pContext->device, pContext->renderImagePass, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->blitVertexShaderModule, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->blitRawFragmentShaderModule, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->blitCRTFragmentShaderModule, nullptr);
	vkDestroyDescriptorSetLayout(pContext->device, pContext->graphicsDescriptorSetLayout, nullptr);
}

static u32 GetDeviceMemoryTypeIndex(u32 typeFilter, VkMemoryPropertyFlags propertyFlags) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(pContext->physicalDevice, &memProperties);

	for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
			return i;
		}
	}
}

static void AllocateMemory(VkMemoryRequirements requirements, VkMemoryPropertyFlags properties, VkDeviceMemory& outMemory) {
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = requirements.size;
	memAllocInfo.memoryTypeIndex = GetDeviceMemoryTypeIndex(requirements.memoryTypeBits, properties);

	VkResult err = vkAllocateMemory(pContext->device, &memAllocInfo, nullptr, &outMemory);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to allocate memory!\n");
	}
}

static void AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, Buffer& outBuffer) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.flags = 0;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult err = vkCreateBuffer(pContext->device, &bufferInfo, nullptr, &outBuffer.buffer);
	if (err != VK_SUCCESS) {
		DEBUG_ERROR("Failed to create buffer!\n");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(pContext->device, outBuffer.buffer, &memRequirements);
	DEBUG_LOG("Buffer memory required: %d\n", memRequirements.size);

	AllocateMemory(memRequirements, memProps, outBuffer.memory);

	vkBindBufferMemory(pContext->device, outBuffer.buffer, outBuffer.memory, 0);
}

static VkCommandBuffer GetTemporaryCommandBuffer() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = pContext->primaryCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(pContext->device, &allocInfo, &commandBuffer);

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

	vkQueueSubmit(pContext->primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(pContext->primaryQueue);

	vkFreeCommandBuffers(pContext->device, pContext->primaryCommandPool, 1, &temp);
}

static void FreeBuffer(Buffer buffer) {
	vkDestroyBuffer(pContext->device, buffer.buffer, nullptr);
	vkFreeMemory(pContext->device, buffer.memory, nullptr);
}

static void CreateImage(u32 width, u32 height, VkImageType type, VkImageUsageFlags usage, Image& outImage) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = type;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	vkCreateImage(pContext->device, &imageInfo, nullptr, &outImage.image);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(pContext->device, outImage.image, &memRequirements);

	AllocateMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImage.memory);
	vkBindImageMemory(pContext->device, outImage.image, outImage.memory, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = outImage.image;
	viewInfo.viewType = (VkImageViewType)type; // A bit sus but probably fine
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(pContext->device, &viewInfo, nullptr, &outImage.view);
}

static void FreeImage(Image image) {
	vkDestroyImageView(pContext->device, image.view, nullptr);
	vkDestroyImage(pContext->device, image.image, nullptr);
	vkFreeMemory(pContext->device, image.memory, nullptr);
}

static VkImageMemoryBarrier GetImageBarrier(Image* pImage, VkImageLayout oldLayout, VkImageLayout newLayout) {
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

static void CreatePalette() {
	u32 paletteData[COLOR_COUNT];
	const VkDeviceSize paletteSize = sizeof(u32) * COLOR_COUNT;

	// TODO: Rethink about this being an util...
	Rendering::Util::GeneratePaletteColors(paletteData);

	Buffer stagingBuffer{};
	AllocateBuffer(paletteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);

	void* data;
	vkMapMemory(pContext->device, stagingBuffer.memory, 0, paletteSize, 0, &data);
	memcpy(data, paletteData, paletteSize);
	vkUnmapMemory(pContext->device, stagingBuffer.memory);
	//free(palBin);

	CreateImage(COLOR_COUNT, 1, VK_IMAGE_TYPE_1D, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, pContext->paletteImage);

	VkCommandBuffer temp = GetTemporaryCommandBuffer();

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(temp, &beginInfo);

	VkImageMemoryBarrier barrier = GetImageBarrier(&pContext->paletteImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdPipelineBarrier(
		temp,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		COLOR_COUNT,
		1,
		1
	};

	vkCmdCopyBufferToImage(
		temp,
		stagingBuffer.buffer,
		pContext->paletteImage.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vkCmdPipelineBarrier(
		temp,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
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

	vkQueueSubmit(pContext->primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(pContext->primaryQueue);

	vkFreeCommandBuffers(pContext->device, pContext->primaryCommandPool, 1, &temp);

	FreeBuffer(stagingBuffer);
}

static constexpr VkDeviceSize PadBufferSize(VkDeviceSize originalSize, const VkDeviceSize minAlignment) {
	VkDeviceSize result = originalSize;
	if (minAlignment > 0) {
		result = (originalSize + minAlignment - 1) & ~(minAlignment - 1);
	}
	return result;
}

static void CreateComputeBuffers() {
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(pContext->physicalDevice, &properties);

	const VkDeviceSize minOffsetAlignment = properties.limits.minStorageBufferOffsetAlignment;

	pContext->paletteTableOffset = 0;
	pContext->paletteTableSize = PadBufferSize(PALETTE_COUNT * sizeof(Palette), minOffsetAlignment);
	pContext->chrOffset = pContext->paletteTableOffset + pContext->paletteTableSize;
	pContext->chrSize = PadBufferSize(CHR_MEMORY_SIZE, minOffsetAlignment);
	pContext->nametableOffset = pContext->chrOffset + pContext->chrSize;
	pContext->nametableSize = PadBufferSize(sizeof(Nametable) * NAMETABLE_COUNT, minOffsetAlignment);
	pContext->oamOffset = pContext->nametableOffset + pContext->nametableSize;
	pContext->oamSize = PadBufferSize(MAX_SPRITE_COUNT * sizeof(Sprite), minOffsetAlignment);
	pContext->renderStateOffset = pContext->oamOffset + pContext->oamSize;
	pContext->renderStateSize = PadBufferSize(sizeof(Scanline) * SCANLINE_COUNT, minOffsetAlignment);
	pContext->computeBufferSize = pContext->paletteTableSize + pContext->chrSize + pContext->nametableSize + pContext->oamSize + pContext->renderStateSize;

	pContext->renderData = calloc(1, pContext->computeBufferSize);

	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		AllocateBuffer(pContext->computeBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, pContext->computeBufferHost[i]);
	}
	AllocateBuffer(pContext->computeBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pContext->computeBufferDevice);
}

static void CopyRenderData() {
	void* temp;
	vkMapMemory(pContext->device, pContext->computeBufferHost[pContext->currentCbIndex].memory, 0, pContext->computeBufferSize, 0, &temp);
	memcpy(temp, pContext->renderData, pContext->computeBufferSize);
	vkUnmapMemory(pContext->device, pContext->computeBufferHost[pContext->currentCbIndex].memory);
}

static void BeginDraw() {
	// Wait for drawing to finish if it hasn't
	vkWaitForFences(pContext->device, 1, &pContext->commandBufferFences[pContext->currentCbIndex], VK_TRUE, UINT64_MAX);

	// Get next swapchain image index
	VkResult err = vkAcquireNextImageKHR(pContext->device, pContext->swapchain, UINT64_MAX, pContext->imageAcquiredSemaphores[pContext->currentCbIndex], VK_NULL_HANDLE, &pContext->currentSwaphainIndex);
	if (err != VK_SUCCESS) {
	}

	vkResetFences(pContext->device, 1, &pContext->commandBufferFences[pContext->currentCbIndex]);
	vkResetCommandBuffer(pContext->primaryCommandBuffers[pContext->currentCbIndex], 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(pContext->primaryCommandBuffers[pContext->currentCbIndex], &beginInfo) != VK_SUCCESS) {
		DEBUG_ERROR("failed to begin recording command buffer!");
	}

	// Should be ready to draw now!
}

static void TransferComputeBufferData() {
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

	VkMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = pContext->computeBufferSize;

	vkCmdCopyBuffer(commandBuffer, pContext->computeBufferHost[pContext->currentCbIndex].buffer, pContext->computeBufferDevice.buffer, 1, &copyRegion);

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

#ifdef EDITOR
static void GetChrImageBarriers(VkImageLayout oldLayout, VkImageLayout newLayout, VkImageMemoryBarrier* outBarriers) {
	for (u32 i = 0; i < PALETTE_COUNT; i++) {
		outBarriers[i] = GetImageBarrier(&pContext->debugChrImage[i], oldLayout, newLayout);
	}
}

static void RenderChrImages(VkCommandBuffer cmd) {
	for (u32 i = 0; i < PALETTE_COUNT; i++) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->chrPipelineLayout, 0, 1, &pContext->chrDescriptorSet[i], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->chrPipeline);
		vkCmdPushConstants(cmd, pContext->chrPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &i);
		vkCmdDispatch(cmd, 32, 16, 1);
	}
}

static void GetPaletteImageBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImageMemoryBarrier* outBarrier) {
	*outBarrier = GetImageBarrier(&pContext->debugPaletteImage, oldLayout, newLayout);
}

static void RenderPaletteImage(VkCommandBuffer cmd) {
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->palettePipelineLayout, 0, 1, &pContext->paletteDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->palettePipeline);
	vkCmdDispatch(cmd, 32, 16, 1);
}
#endif

static void RunSoftwareRenderer() {
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

	// Transfer images to compute writeable layout
	// Compute won't happen before this is all done

#ifdef EDITOR
	constexpr u32 barrierCount = 1 + PALETTE_COUNT + 1;
#else
	constexpr u32 barrierCount = 1;
#endif
	VkImageMemoryBarrier barriers[barrierCount]{};

	barriers[0] = GetImageBarrier(&pContext->colorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

#ifdef EDITOR
	GetChrImageBarriers(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, barriers + 1);
	GetPaletteImageBarrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, barriers + 1 + PALETTE_COUNT);
#endif

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		barrierCount, barriers
	);

	// Clear old scanline data
	vkCmdFillBuffer(
		commandBuffer,
		pContext->scanlineBuffer.buffer,
		0,
		sizeof(ScanlineData) * SCANLINE_COUNT,
		0
	);

	// Do all the asynchronous compute stuff
#ifdef EDITOR
	RenderChrImages(commandBuffer);
	RenderPaletteImage(commandBuffer);
#endif

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->evaluatePipelineLayout, 0, 1, &pContext->computeDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->evaluatePipeline);
	vkCmdDispatch(commandBuffer, MAX_SPRITE_COUNT / MAX_SPRITES_PER_SCANLINE, SCANLINE_COUNT, 1);

	// Wait for scanline buffer to be written before running software renderer for the final image
	VkBufferMemoryBarrier scanlineBarrier{};
	scanlineBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	scanlineBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	scanlineBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	scanlineBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	scanlineBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	scanlineBarrier.buffer = pContext->scanlineBuffer.buffer;
	scanlineBarrier.offset = 0;
	scanlineBarrier.size = sizeof(ScanlineData) * SCANLINE_COUNT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, &scanlineBarrier,
		0, nullptr
	);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->softwarePipelineLayout, 0, 1, &pContext->computeDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->softwarePipeline);
	vkCmdDispatch(commandBuffer, VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS / 32, VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS / 32, 1);

	// Transfer images to shader readable layout
	barriers[0] = GetImageBarrier(&pContext->colorImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#ifdef EDITOR
	GetChrImageBarriers(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, barriers + 1);
	GetPaletteImageBarrier(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, barriers + 1 + PALETTE_COUNT);
#endif

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		barrierCount, barriers
	);
}

static void BeginRenderPass() {
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

	VkFramebuffer framebuffer = pContext->swapchainFramebuffers[pContext->currentSwaphainIndex];
	VkExtent2D extent = pContext->surfaceCapabilities.currentExtent;

	VkRenderPassBeginInfo renderImagePassBeginInfo{};
	renderImagePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderImagePassBeginInfo.renderPass = pContext->renderImagePass;
	renderImagePassBeginInfo.framebuffer = framebuffer;
	renderImagePassBeginInfo.renderArea.offset = { 0, 0 };
	renderImagePassBeginInfo.renderArea.extent = extent;
	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	renderImagePassBeginInfo.clearValueCount = 1;
	renderImagePassBeginInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderImagePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void BlitSoftwareResults(const Quad& quad) {
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];
	VkExtent2D extent = pContext->surfaceCapabilities.currentExtent;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pContext->settings.useCRTFilter ? pContext->blitCRTPipeline : pContext->blitRawPipeline);

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

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pContext->blitPipelineLayout, 0, 1, &pContext->graphicsDescriptorSets[pContext->currentCbIndex], 0, nullptr);

	vkCmdPushConstants(commandBuffer, pContext->blitPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Quad), &quad);
	vkCmdDraw(commandBuffer, 4, 1, 0, 0);
}

static void EndRenderPass() {
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

#ifdef EDITOR
	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData != nullptr) {
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}
#endif

	vkCmdEndRenderPass(commandBuffer);
}

static void EndDraw() {
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		DEBUG_ERROR("failed to record command buffer!");
	}

	// Submit the above commands
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore waitSemaphores[] = { pContext->imageAcquiredSemaphores[pContext->currentCbIndex] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &pContext->primaryCommandBuffers[pContext->currentCbIndex];
	VkSemaphore signalSemaphores[] = { pContext->drawCompleteSemaphores[pContext->currentCbIndex] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	VkResult err = vkQueueSubmit(pContext->primaryQueue, 1, &submitInfo, pContext->commandBufferFences[pContext->currentCbIndex]);

	// Present to swapchain
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapchains[] = { pContext->swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &pContext->currentSwaphainIndex;
	presentInfo.pResults = nullptr; // Optional

	vkQueuePresentKHR(pContext->primaryQueue, &presentInfo);

	// Advance cb index
	pContext->currentCbIndex = (pContext->currentCbIndex + 1) % COMMAND_BUFFER_COUNT;
	// Advance swapchain index
	pContext->currentSwaphainIndex = (pContext->currentSwaphainIndex + 1) % SWAPCHAIN_IMAGE_COUNT;
}

////////////////////////////////////////////////////////////

void Rendering::CreateContext() {
	// TODO: Handle if context already exists
	pContext = new RenderContext;
	assert(pContext != nullptr);
	
	CreateVulkanInstance();
}

// TODO: Support other types of surfaces later?
void Rendering::CreateSurface(SDL_Window* sdlWindow) {
	pContext->surface = VK_NULL_HANDLE;
	SDL_Vulkan_CreateSurface(sdlWindow, pContext->instance, &pContext->surface);
}

void Rendering::Init() {
	GetSuitablePhysicalDevice();
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pContext->physicalDevice, pContext->surface, &pContext->surfaceCapabilities);
	CreateDevice();
	vkGetDeviceQueue(pContext->device, pContext->primaryQueueFamilyIndex, 0, &pContext->primaryQueue);
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

	vkCreateDescriptorPool(pContext->device, &descriptorPoolInfo, nullptr, &pContext->descriptorPool);

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

	vkCreateDescriptorSetLayout(pContext->device, &layoutInfo, nullptr, &pContext->graphicsDescriptorSetLayout);

	VkDescriptorSetLayout layouts[COMMAND_BUFFER_COUNT]{};
	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		layouts[i] = pContext->graphicsDescriptorSetLayout;
	}
	VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
	descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
	descriptorSetAllocInfo.descriptorSetCount = COMMAND_BUFFER_COUNT;
	descriptorSetAllocInfo.pSetLayouts = layouts;

	vkAllocateDescriptorSets(pContext->device, &descriptorSetAllocInfo, pContext->graphicsDescriptorSets);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = pContext->primaryQueueFamilyIndex;

	if (vkCreateCommandPool(pContext->device, &poolInfo, nullptr, &pContext->primaryCommandPool) != VK_SUCCESS) {
		DEBUG_ERROR("failed to create command pool!");
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = pContext->primaryCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = COMMAND_BUFFER_COUNT;

	if (vkAllocateCommandBuffers(pContext->device, &allocInfo, pContext->primaryCommandBuffers) != VK_SUCCESS) {
		DEBUG_ERROR("failed to allocate command buffers!");
	}

	pContext->currentCbIndex = 0;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		vkCreateSemaphore(pContext->device, &semaphoreInfo, nullptr, &pContext->imageAcquiredSemaphores[i]);
		vkCreateSemaphore(pContext->device, &semaphoreInfo, nullptr, &pContext->drawCompleteSemaphores[i]);
		vkCreateFence(pContext->device, &fenceInfo, nullptr, &pContext->commandBufferFences[i]);
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

	vkCreateSampler(pContext->device, &samplerInfo, nullptr, &pContext->defaultSampler);

	// Compute resources
	CreatePalette();
	CreateComputeBuffers();
	AllocateBuffer(sizeof(ScanlineData) * SCANLINE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pContext->scanlineBuffer);

	CreateImage(VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS, VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, pContext->colorImage);

	// Write into descriptor sets...
	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = pContext->colorImage.view;
		imageInfo.sampler = pContext->defaultSampler;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = pContext->graphicsDescriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(pContext->device, 1, &descriptorWrite, 0, nullptr);
	}

	// COMPUTE

	// Descriptors
	VkDescriptorSetLayoutBinding storageLayoutBinding{};
	storageLayoutBinding.binding = 0;
	storageLayoutBinding.descriptorCount = 1;
	storageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageLayoutBinding.pImmutableSamplers = nullptr;
	storageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding paletteLayoutBinding{};
	paletteLayoutBinding.binding = 1;
	paletteLayoutBinding.descriptorCount = 1;
	paletteLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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

	VkDescriptorSetLayoutBinding nametableBinding{};
	nametableBinding.binding = 4;
	nametableBinding.descriptorCount = 1;
	nametableBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	nametableBinding.pImmutableSamplers = nullptr;
	nametableBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding oamBinding{};
	oamBinding.binding = 5;
	oamBinding.descriptorCount = 1;
	oamBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	oamBinding.pImmutableSamplers = nullptr;
	oamBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding scanlineBinding{};
	scanlineBinding.binding = 6;
	scanlineBinding.descriptorCount = 1;
	scanlineBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	scanlineBinding.pImmutableSamplers = nullptr;
	scanlineBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding renderStateBinding{};
	renderStateBinding.binding = 7;
	renderStateBinding.descriptorCount = 1;
	renderStateBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	renderStateBinding.pImmutableSamplers = nullptr;
	renderStateBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding bindings[] = { storageLayoutBinding, paletteLayoutBinding, chrLayoutBinding, palTableBinding, nametableBinding, oamBinding, scanlineBinding, renderStateBinding };
	VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
	computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	computeLayoutInfo.bindingCount = 8;
	computeLayoutInfo.pBindings = bindings;

	vkCreateDescriptorSetLayout(pContext->device, &computeLayoutInfo, nullptr, &pContext->computeDescriptorSetLayout);

	VkDescriptorSetAllocateInfo computeDescriptorSetAllocInfo{};
	computeDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	computeDescriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
	computeDescriptorSetAllocInfo.descriptorSetCount = 1;
	computeDescriptorSetAllocInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;

	vkAllocateDescriptorSets(pContext->device, &computeDescriptorSetAllocInfo, &pContext->computeDescriptorSet);

	// Compute evaluate
	VkPipelineLayoutCreateInfo evaluatePipelineLayoutInfo{};
	evaluatePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	evaluatePipelineLayoutInfo.setLayoutCount = 1;
	evaluatePipelineLayoutInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;

	vkCreatePipelineLayout(pContext->device, &evaluatePipelineLayoutInfo, nullptr, &pContext->evaluatePipelineLayout);

	u32 evaluateShaderLength;
	char* evaluateShader = AllocFileBytes("assets/shaders/scanline_evaluate.spv", evaluateShaderLength);
	pContext->evaluateShaderModule = CreateShaderModule(pContext->device, evaluateShader, evaluateShaderLength);
	free(evaluateShader);

	VkPipelineShaderStageCreateInfo evaluateShaderStageInfo{};
	evaluateShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	evaluateShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	evaluateShaderStageInfo.module = pContext->evaluateShaderModule;
	evaluateShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo evaluateCreateInfo{};
	evaluateCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	evaluateCreateInfo.flags = 0;
	evaluateCreateInfo.stage = evaluateShaderStageInfo;
	evaluateCreateInfo.layout = pContext->evaluatePipelineLayout;

	vkCreateComputePipelines(pContext->device, VK_NULL_HANDLE, 1, &evaluateCreateInfo, nullptr, &pContext->evaluatePipeline);

	// Compute rendering
	VkPipelineLayoutCreateInfo softwarePipelineLayoutInfo{};
	softwarePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	softwarePipelineLayoutInfo.setLayoutCount = 1;
	softwarePipelineLayoutInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;
	softwarePipelineLayoutInfo.pPushConstantRanges = nullptr;
	softwarePipelineLayoutInfo.pushConstantRangeCount = 0;

	vkCreatePipelineLayout(pContext->device, &softwarePipelineLayoutInfo, nullptr, &pContext->softwarePipelineLayout);

	// TODO: Handling files here is bad, move somewhere else
	u32 softwareShaderLength;
	char* softwareShader = AllocFileBytes("assets/shaders/software.spv", softwareShaderLength);
	pContext->softwareShaderModule = CreateShaderModule(pContext->device, softwareShader, softwareShaderLength);
	free(softwareShader);

	VkPipelineShaderStageCreateInfo compShaderStageInfo{};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = pContext->softwareShaderModule;
	compShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo computeCreateInfo{};
	computeCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computeCreateInfo.flags = 0;
	computeCreateInfo.stage = compShaderStageInfo;
	computeCreateInfo.layout = pContext->softwarePipelineLayout;

	vkCreateComputePipelines(pContext->device, VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, &pContext->softwarePipeline);

	VkDescriptorImageInfo perkele{};
	perkele.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	perkele.imageView = pContext->colorImage.view;
	perkele.sampler = pContext->defaultSampler;

	VkDescriptorImageInfo paletteBufferInfo{};
	paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	paletteBufferInfo.imageView = pContext->paletteImage.view;
	paletteBufferInfo.sampler = pContext->defaultSampler;

	VkDescriptorBufferInfo chrBufferInfo{};
	chrBufferInfo.buffer = pContext->computeBufferDevice.buffer;
	chrBufferInfo.offset = pContext->chrOffset;
	chrBufferInfo.range = pContext->chrSize;

	VkDescriptorBufferInfo palTableInfo{};
	palTableInfo.buffer = pContext->computeBufferDevice.buffer;
	palTableInfo.offset = pContext->paletteTableOffset;
	palTableInfo.range = pContext->paletteTableSize;

	VkDescriptorBufferInfo nametableInfo{};
	nametableInfo.buffer = pContext->computeBufferDevice.buffer;
	nametableInfo.offset = pContext->nametableOffset;
	nametableInfo.range = pContext->nametableSize;

	VkDescriptorBufferInfo oamInfo{};
	oamInfo.buffer = pContext->computeBufferDevice.buffer;
	oamInfo.offset = pContext->oamOffset;
	oamInfo.range = pContext->oamSize;

	VkDescriptorBufferInfo scanlineInfo{};
	scanlineInfo.buffer = pContext->scanlineBuffer.buffer;
	scanlineInfo.offset = 0;
	scanlineInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo renderStateInfo{};
	renderStateInfo.buffer = pContext->computeBufferDevice.buffer;
	renderStateInfo.offset = pContext->renderStateOffset;
	renderStateInfo.range = pContext->renderStateSize;

	VkWriteDescriptorSet descriptorWrite[8]{};
	descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[0].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[0].dstBinding = 0;
	descriptorWrite[0].dstArrayElement = 0;
	descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite[0].descriptorCount = 1;
	descriptorWrite[0].pImageInfo = &perkele;

	descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[1].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[1].dstBinding = 1;
	descriptorWrite[1].dstArrayElement = 0;
	descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite[1].descriptorCount = 1;
	descriptorWrite[1].pImageInfo = &paletteBufferInfo;

	descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[2].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[2].dstBinding = 2;
	descriptorWrite[2].dstArrayElement = 0;
	descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[2].descriptorCount = 1;
	descriptorWrite[2].pBufferInfo = &chrBufferInfo;

	descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[3].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[3].dstBinding = 3;
	descriptorWrite[3].dstArrayElement = 0;
	descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[3].descriptorCount = 1;
	descriptorWrite[3].pBufferInfo = &palTableInfo;

	descriptorWrite[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[4].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[4].dstBinding = 4;
	descriptorWrite[4].dstArrayElement = 0;
	descriptorWrite[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[4].descriptorCount = 1;
	descriptorWrite[4].pBufferInfo = &nametableInfo;

	descriptorWrite[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[5].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[5].dstBinding = 5;
	descriptorWrite[5].dstArrayElement = 0;
	descriptorWrite[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[5].descriptorCount = 1;
	descriptorWrite[5].pBufferInfo = &oamInfo;

	descriptorWrite[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[6].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[6].dstBinding = 6;
	descriptorWrite[6].dstArrayElement = 0;
	descriptorWrite[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[6].descriptorCount = 1;
	descriptorWrite[6].pBufferInfo = &scanlineInfo;

	descriptorWrite[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[7].dstSet = pContext->computeDescriptorSet;
	descriptorWrite[7].dstBinding = 7;
	descriptorWrite[7].dstArrayElement = 0;
	descriptorWrite[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[7].descriptorCount = 1;
	descriptorWrite[7].pBufferInfo = &renderStateInfo;

	vkUpdateDescriptorSets(pContext->device, 8, descriptorWrite, 0, nullptr);

	pContext->settings = DEFAULT_RENDER_SETTINGS;
}

void Rendering::Free() {
	// Wait for all commands to execute first
	WaitForAllCommands();

	// Free imgui stuff
	vkDestroyDescriptorPool(pContext->device, pContext->imGuiDescriptorPool, nullptr);

	// vkFreeDescriptorSets(pContext->device, pContext->descriptorPool, COMMAND_BUFFER_COUNT, pContext->graphicsDescriptorSets);
	// vkFreeDescriptorSets(pContext->device, pContext->descriptorPool, 1, &pContext->computeDescriptorSet);
	vkDestroyDescriptorPool(pContext->device, pContext->descriptorPool, nullptr);

	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		vkDestroySemaphore(pContext->device, pContext->imageAcquiredSemaphores[i], nullptr);
		vkDestroySemaphore(pContext->device, pContext->drawCompleteSemaphores[i], nullptr);
		vkDestroyFence(pContext->device, pContext->commandBufferFences[i], nullptr);
	}
	vkFreeCommandBuffers(pContext->device, pContext->primaryCommandPool, COMMAND_BUFFER_COUNT, pContext->primaryCommandBuffers);
	vkDestroyCommandPool(pContext->device, pContext->primaryCommandPool, nullptr);

	FreeSwapchain();

	FreeGraphicsPipeline();

	vkDestroySampler(pContext->device, pContext->defaultSampler, nullptr);
	vkDestroyDescriptorSetLayout(pContext->device, pContext->computeDescriptorSetLayout, nullptr);

	vkDestroyPipeline(pContext->device, pContext->softwarePipeline, nullptr);
	vkDestroyPipelineLayout(pContext->device, pContext->softwarePipelineLayout, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->softwareShaderModule, nullptr);

	vkDestroyPipeline(pContext->device, pContext->evaluatePipeline, nullptr);
	vkDestroyPipelineLayout(pContext->device, pContext->evaluatePipelineLayout, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->evaluateShaderModule, nullptr);
	FreeImage(pContext->colorImage);
	FreeImage(pContext->paletteImage);

#ifdef EDITOR
	vkDestroyPipeline(pContext->device, pContext->chrPipeline, nullptr);
	vkDestroyPipelineLayout(pContext->device, pContext->chrPipelineLayout, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->chrShaderModule, nullptr);
	for (int i = 0; i < PALETTE_COUNT; i++) {
		FreeImage(pContext->debugChrImage[i]);
	}

	vkDestroyPipeline(pContext->device, pContext->palettePipeline, nullptr);
	vkDestroyPipelineLayout(pContext->device, pContext->palettePipelineLayout, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->paletteShaderModule, nullptr);
	FreeImage(pContext->debugPaletteImage);
#endif

	FreeBuffer(pContext->computeBufferDevice);
	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		FreeBuffer(pContext->computeBufferHost[i]);
	}
	free(pContext->renderData);
	FreeBuffer(pContext->scanlineBuffer);

	vkDestroyDevice(pContext->device, nullptr);
	vkDestroySurfaceKHR(pContext->instance, pContext->surface, nullptr);
	vkDestroyInstance(pContext->instance, nullptr);
}

void Rendering::DestroyContext() {
	if (pContext == nullptr) {
		return;
	}

	delete pContext;
	pContext = nullptr;
}

//////////////////////////////////////////////////////

void Rendering::Render() {
	// Just run all the commands
	CopyRenderData();
	BeginDraw();
	TransferComputeBufferData();
	RunSoftwareRenderer();
	BeginRenderPass();
	BlitSoftwareResults(DEFAULT_QUAD);
	EndRenderPass();
	EndDraw();
}

void Rendering::WaitForAllCommands() {
	vkWaitForFences(pContext->device, COMMAND_BUFFER_COUNT, pContext->commandBufferFences, VK_TRUE, UINT64_MAX);
}

void Rendering::ResizeSurface(u32 width, u32 height) {
	// Wait for all commands to execute first
	vkWaitForFences(pContext->device, COMMAND_BUFFER_COUNT, pContext->commandBufferFences, VK_TRUE, UINT64_MAX);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pContext->physicalDevice, pContext->surface, &pContext->surfaceCapabilities);
	FreeSwapchain();
	CreateSwapchain();
}

//////////////////////////////////////////////////////

RenderSettings* Rendering::GetSettingsPtr() {
	return &pContext->settings;
}
Palette* Rendering::GetPalettePtr(u32 paletteIndex) {
	if (paletteIndex >= PALETTE_COUNT) {
		return nullptr;
	}

	Palette* pal = (Palette*)((u8*)pContext->renderData + pContext->paletteTableOffset);
	return pal + paletteIndex;
}
Sprite* Rendering::GetSpritesPtr(u32 offset) {
	if (offset >= MAX_SPRITE_COUNT) {
		return nullptr;
	}

	Sprite* spr = (Sprite*)((u8*)pContext->renderData + pContext->oamOffset);
	return spr + offset;
}
ChrSheet* Rendering::GetChrPtr(u32 sheetIndex) {
	if (sheetIndex >= CHR_COUNT) {
		return nullptr;
	}

	ChrSheet* sheet = (ChrSheet*)((u8*)pContext->renderData + pContext->chrOffset);
	return sheet + sheetIndex;
}
Nametable* Rendering::GetNametablePtr(u32 index) {
	if (index >= NAMETABLE_COUNT) {
		return nullptr;
	}

	Nametable* tbl = (Nametable*)((u8*)pContext->renderData + pContext->nametableOffset);
	return tbl + index;
}
Scanline* Rendering::GetScanlinePtr(u32 offset) {
	if (offset >= SCANLINE_COUNT) {
		return nullptr;
	}

	Scanline* scanlines = (Scanline*)((u8*)pContext->renderData + pContext->renderStateOffset);
	return scanlines + offset;
}

//////////////////////////////////////////////////////
#ifdef EDITOR
void Rendering::InitImGui(SDL_Window* pWindow) {
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

	vkCreateDescriptorPool(pContext->device, &poolInfo, nullptr, &pContext->imGuiDescriptorPool);

	ImGui_ImplVulkan_InitInfo vulkanInitInfo{};
	vulkanInitInfo.Instance = pContext->instance;
	vulkanInitInfo.PhysicalDevice = pContext->physicalDevice;
	vulkanInitInfo.Device = pContext->device;
	vulkanInitInfo.QueueFamily = pContext->primaryQueueFamilyIndex;
	vulkanInitInfo.Queue = pContext->primaryQueue;
	vulkanInitInfo.DescriptorPool = pContext->imGuiDescriptorPool;
	vulkanInitInfo.MinImageCount = SWAPCHAIN_IMAGE_COUNT;
	vulkanInitInfo.ImageCount = SWAPCHAIN_IMAGE_COUNT;
	vulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplSDL2_InitForVulkan(pWindow);
	ImGui_ImplVulkan_Init(&vulkanInitInfo, pContext->renderImagePass);
	ImGui_ImplVulkan_CreateFontsTexture();

}
void Rendering::BeginImGuiFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
}
void Rendering::ShutdownImGui() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL2_Shutdown();
}

void Rendering::CreateImGuiChrTextures(ImTextureID* pTextures) {
	VkDescriptorSetLayout layouts[PALETTE_COUNT]{};
	for (u32 i = 0; i < PALETTE_COUNT; i++) {
		layouts[i] = pContext->computeDescriptorSetLayout;
	}

	VkDescriptorSetAllocateInfo chrDescriptorSetAllocInfo{};
	chrDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	chrDescriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
	chrDescriptorSetAllocInfo.descriptorSetCount = PALETTE_COUNT;
	chrDescriptorSetAllocInfo.pSetLayouts = layouts;

	VkResult res = vkAllocateDescriptorSets(pContext->device, &chrDescriptorSetAllocInfo, pContext->chrDescriptorSet);
	if (res != VK_SUCCESS) {
		DEBUG_ERROR("Whoopsie poopsie :c %d\n", res);
	}

	for (u32 i = 0; i < PALETTE_COUNT; i++) {
		CreateImage(CHR_COUNT * CHR_DIM_PIXELS, CHR_DIM_PIXELS, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, pContext->debugChrImage[i]);

		pTextures[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(pContext->defaultSampler, pContext->debugChrImage[i].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		////////////////////////////////////////////////////

		VkDescriptorImageInfo perkele{};
		perkele.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		perkele.imageView = pContext->debugChrImage[i].view;
		perkele.sampler = pContext->defaultSampler;

		VkDescriptorImageInfo paletteBufferInfo{};
		paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		paletteBufferInfo.imageView = pContext->paletteImage.view;
		paletteBufferInfo.sampler = pContext->defaultSampler;

		VkDescriptorBufferInfo chrBufferInfo{};
		chrBufferInfo.buffer = pContext->computeBufferDevice.buffer;
		chrBufferInfo.offset = pContext->chrOffset;
		chrBufferInfo.range = pContext->chrSize;

		VkDescriptorBufferInfo palTableInfo{};
		palTableInfo.buffer = pContext->computeBufferDevice.buffer;
		palTableInfo.offset = pContext->paletteTableOffset;
		palTableInfo.range = pContext->paletteTableSize;

		VkWriteDescriptorSet descriptorWrite[4]{};
		descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[0].dstSet = pContext->chrDescriptorSet[i];
		descriptorWrite[0].dstBinding = 0;
		descriptorWrite[0].dstArrayElement = 0;
		descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite[0].descriptorCount = 1;
		descriptorWrite[0].pImageInfo = &perkele;

		descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[1].dstSet = pContext->chrDescriptorSet[i];
		descriptorWrite[1].dstBinding = 1;
		descriptorWrite[1].dstArrayElement = 0;
		descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite[1].descriptorCount = 1;
		descriptorWrite[1].pImageInfo = &paletteBufferInfo;

		descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[2].dstSet = pContext->chrDescriptorSet[i];
		descriptorWrite[2].dstBinding = 2;
		descriptorWrite[2].dstArrayElement = 0;
		descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[2].descriptorCount = 1;
		descriptorWrite[2].pBufferInfo = &chrBufferInfo;

		descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[3].dstSet = pContext->chrDescriptorSet[i];
		descriptorWrite[3].dstBinding = 3;
		descriptorWrite[3].dstArrayElement = 0;
		descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[3].descriptorCount = 1;
		descriptorWrite[3].pBufferInfo = &palTableInfo;

		vkUpdateDescriptorSets(pContext->device, 4, descriptorWrite, 0, nullptr);
	}

	// Selected palette as push constant
	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(u32);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPipelineLayoutCreateInfo chrPipelineLayoutInfo{};
	chrPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	chrPipelineLayoutInfo.setLayoutCount = 1;
	chrPipelineLayoutInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;
	chrPipelineLayoutInfo.pushConstantRangeCount = 1;
	chrPipelineLayoutInfo.pPushConstantRanges = &pushConstant;

	vkCreatePipelineLayout(pContext->device, &chrPipelineLayoutInfo, nullptr, &pContext->chrPipelineLayout);

	u32 chrShaderLength;
	char* chrShader = AllocFileBytes("assets/shaders/debug_blit_chr.spv", chrShaderLength);
	pContext->chrShaderModule = CreateShaderModule(pContext->device, chrShader, chrShaderLength);
	free(chrShader);

	VkPipelineShaderStageCreateInfo chrShaderStageInfo{};
	chrShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	chrShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	chrShaderStageInfo.module = pContext->chrShaderModule;
	chrShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo chrCreateInfo{};
	chrCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	chrCreateInfo.flags = 0;
	chrCreateInfo.stage = chrShaderStageInfo;
	chrCreateInfo.layout = pContext->chrPipelineLayout;

	vkCreateComputePipelines(pContext->device, VK_NULL_HANDLE, 1, &chrCreateInfo, nullptr, &pContext->chrPipeline);
}

void Rendering::FreeImGuiChrTextures(ImTextureID* pTextures) {
	for (u32 i = 0; i < PALETTE_COUNT; i++) {
		ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)pTextures[i]);
	}
}

void Rendering::CreateImGuiPaletteTexture(ImTextureID* pTexture) {
	CreateImage(PALETTE_MEMORY_SIZE, 1, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, pContext->debugPaletteImage);

	VkPipelineLayoutCreateInfo chrPipelineLayoutInfo{};
	chrPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	chrPipelineLayoutInfo.setLayoutCount = 1;
	chrPipelineLayoutInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;

	vkCreatePipelineLayout(pContext->device, &chrPipelineLayoutInfo, nullptr, &pContext->palettePipelineLayout);

	u32 chrShaderLength;
	char* chrShader = AllocFileBytes("assets/shaders/debug_blit_pal.spv", chrShaderLength);
	pContext->paletteShaderModule = CreateShaderModule(pContext->device, chrShader, chrShaderLength);
	free(chrShader);

	VkPipelineShaderStageCreateInfo chrShaderStageInfo{};
	chrShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	chrShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	chrShaderStageInfo.module = pContext->paletteShaderModule;
	chrShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo chrCreateInfo{};
	chrCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	chrCreateInfo.flags = 0;
	chrCreateInfo.stage = chrShaderStageInfo;
	chrCreateInfo.layout = pContext->palettePipelineLayout;

	vkCreateComputePipelines(pContext->device, VK_NULL_HANDLE, 1, &chrCreateInfo, nullptr, &pContext->palettePipeline);

	////////////////////////////////////////

	VkDescriptorSetAllocateInfo chrDescriptorSetAllocInfo{};
	chrDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	chrDescriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
	chrDescriptorSetAllocInfo.descriptorSetCount = 1;
	chrDescriptorSetAllocInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;

	vkAllocateDescriptorSets(pContext->device, &chrDescriptorSetAllocInfo, &pContext->paletteDescriptorSet);

	////////////////////////////////////////////////////

	VkDescriptorImageInfo perkele{};
	perkele.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	perkele.imageView = pContext->debugPaletteImage.view;
	perkele.sampler = pContext->defaultSampler;

	VkDescriptorImageInfo paletteBufferInfo{};
	paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	paletteBufferInfo.imageView = pContext->paletteImage.view;
	paletteBufferInfo.sampler = pContext->defaultSampler;

	VkDescriptorBufferInfo chrBufferInfo{};
	chrBufferInfo.buffer = pContext->computeBufferDevice.buffer;
	chrBufferInfo.offset = pContext->chrOffset;
	chrBufferInfo.range = pContext->chrSize;

	VkDescriptorBufferInfo palTableInfo{};
	palTableInfo.buffer = pContext->computeBufferDevice.buffer;
	palTableInfo.offset = pContext->paletteTableOffset;
	palTableInfo.range = pContext->paletteTableSize;

	VkWriteDescriptorSet descriptorWrite[4]{};
	descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[0].dstSet = pContext->paletteDescriptorSet;
	descriptorWrite[0].dstBinding = 0;
	descriptorWrite[0].dstArrayElement = 0;
	descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite[0].descriptorCount = 1;
	descriptorWrite[0].pImageInfo = &perkele;

	descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[1].dstSet = pContext->paletteDescriptorSet;
	descriptorWrite[1].dstBinding = 1;
	descriptorWrite[1].dstArrayElement = 0;
	descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite[1].descriptorCount = 1;
	descriptorWrite[1].pImageInfo = &paletteBufferInfo;

	descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[2].dstSet = pContext->paletteDescriptorSet;
	descriptorWrite[2].dstBinding = 2;
	descriptorWrite[2].dstArrayElement = 0;
	descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[2].descriptorCount = 1;
	descriptorWrite[2].pBufferInfo = &chrBufferInfo;

	descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite[3].dstSet = pContext->paletteDescriptorSet;
	descriptorWrite[3].dstBinding = 3;
	descriptorWrite[3].dstArrayElement = 0;
	descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite[3].descriptorCount = 1;
	descriptorWrite[3].pBufferInfo = &palTableInfo;

	vkUpdateDescriptorSets(pContext->device, 4, descriptorWrite, 0, nullptr);

	*pTexture = (ImTextureID)ImGui_ImplVulkan_AddTexture(pContext->defaultSampler, pContext->debugPaletteImage.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Rendering::FreeImGuiPaletteTexture(ImTextureID* pTexture) {
	ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)*pTexture);
}

void Rendering::CreateImGuiGameTexture(ImTextureID* pTexture) {
	*pTexture = (ImTextureID)ImGui_ImplVulkan_AddTexture(pContext->defaultSampler, pContext->colorImage.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Rendering::FreeImGuiGameTexture(ImTextureID* pTexture) {
	ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)*pTexture);
}
#endif