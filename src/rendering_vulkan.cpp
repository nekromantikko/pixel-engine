#ifdef PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <cstring>
#include "rendering.h"
#include "rendering_util.h"
#include "debug.h"
#include <SDL_vulkan.h>
#include <cassert>
#include <vector>
#include "shader.h"

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
	VkSampler paletteSampler;

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
	Buffer computeBufferDevice[COMMAND_BUFFER_COUNT];
	Buffer computeStagingBuffers[COMMAND_BUFFER_COUNT];
	VkFence renderDataCopyFence;

	Buffer scanlineBuffers[COMMAND_BUFFER_COUNT];

	Image colorImages[COMMAND_BUFFER_COUNT];

	VkDescriptorSetLayout computeDescriptorSetLayout;
	VkDescriptorSet computeDescriptorSets[COMMAND_BUFFER_COUNT];
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
	VkDescriptorSetLayout debugDescriptorSetLayout;

	VkPipelineLayout chrPipelineLayout;
	VkPipeline chrPipeline;
	VkShaderModule chrShaderModule;

	VkPipelineLayout palettePipelineLayout;
	VkPipeline palettePipeline;
	VkShaderModule paletteShaderModule;
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

static VkShaderModule CreateShaderModule(const char* fname) {
	std::filesystem::path path(fname);

	u32 size;
	if (!Assets::LoadShaderFromFile(path, size)) {
		DEBUG_ERROR("Failed to load shader from file (%s)\n", path.string().c_str());
		return VK_NULL_HANDLE;
	}
	char* data = (char*)malloc(size);
	if (!data) {
		DEBUG_ERROR("Failed to allocate memory for shader (%s)\n", path.string().c_str());
		return VK_NULL_HANDLE;

	}
	if (!Assets::LoadShaderFromFile(path, size, data)) {
		DEBUG_ERROR("Failed to load shader from file (%s)\n", path.string().c_str());
		free(data);
		return VK_NULL_HANDLE;
	}
	return CreateShaderModule(pContext->device, data, size);
}

static void CreateGraphicsPipeline()
{
	pContext->blitVertexShaderModule = CreateShaderModule("shaders/quad.spv");
	pContext->blitRawFragmentShaderModule = CreateShaderModule("shaders/textured_raw.spv");
	pContext->blitCRTFragmentShaderModule = CreateShaderModule("shaders/textured_crt.spv");

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

	outBuffer.size = size;
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
	if (buffer.memory != VK_NULL_HANDLE) {
		vkFreeMemory(pContext->device, buffer.memory, nullptr);
	}
}

static void CreateImage(u32 width, u32 height, VkImageType type, VkImageUsageFlags usage, Image& outImage, bool srgb = false) {
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

	vkCreateImage(pContext->device, &imageInfo, nullptr, &outImage.image);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(pContext->device, outImage.image, &memRequirements);

	AllocateMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImage.memory);
	vkBindImageMemory(pContext->device, outImage.image, outImage.memory, 0);

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

	vkCreateImageView(pContext->device, &viewInfo, nullptr, &outImage.view);

	outImage.width = width;
	outImage.height = height;
}

static void FreeImage(Image image) {
	vkDestroyImageView(pContext->device, image.view, nullptr);
	vkDestroyImage(pContext->device, image.image, nullptr);
	vkFreeMemory(pContext->device, image.memory, nullptr);
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

	CreateImage(COLOR_COUNT, 1, VK_IMAGE_TYPE_1D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, pContext->paletteImage, true);

	// Copy buffer to image
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

	// Create special sampler for the palette
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
	samplerInfo.unnormalizedCoordinates = VK_TRUE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	vkCreateSampler(pContext->device, &samplerInfo, nullptr, &pContext->paletteSampler);
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
		AllocateBuffer(pContext->computeBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pContext->computeBufferDevice[i]);
		AllocateBuffer(pContext->computeBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, pContext->computeStagingBuffers[i]);
	}
}

static void CopyRenderData() {
	void* temp;
	vkMapMemory(pContext->device, pContext->computeStagingBuffers[pContext->currentCbIndex].memory, 0, pContext->computeBufferSize, 0, &temp);
	memcpy(temp, pContext->renderData, pContext->computeBufferSize);
	vkUnmapMemory(pContext->device, pContext->computeStagingBuffers[pContext->currentCbIndex].memory);
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

	vkCmdCopyBuffer(commandBuffer, pContext->computeStagingBuffers[pContext->currentCbIndex].buffer, pContext->computeBufferDevice[pContext->currentCbIndex].buffer, 1, &copyRegion);

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

static void RunSoftwareRenderer() {
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

	// Transfer images to compute writeable layout
	// Compute won't happen before this is all done

	VkImageMemoryBarrier barrier = GetImageBarrier(&pContext->colorImages[pContext->currentCbIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	// Clear old scanline data
	vkCmdFillBuffer(
		commandBuffer,
		pContext->scanlineBuffers[pContext->currentCbIndex].buffer,
		0,
		sizeof(ScanlineData) * SCANLINE_COUNT,
		0
	);

	// Do all the asynchronous compute stuff
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->evaluatePipelineLayout, 0, 1, &pContext->computeDescriptorSets[pContext->currentCbIndex], 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->evaluatePipeline);
	vkCmdDispatch(commandBuffer, MAX_SPRITE_COUNT / MAX_SPRITES_PER_SCANLINE, SCANLINE_COUNT, 1);

	// Wait for scanline buffer to be written before running software renderer for the final image
	VkBufferMemoryBarrier scanlineBarrier{};
	scanlineBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	scanlineBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	scanlineBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	scanlineBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	scanlineBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	scanlineBarrier.buffer = pContext->scanlineBuffers[pContext->currentCbIndex].buffer;
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

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->softwarePipelineLayout, 0, 1, &pContext->computeDescriptorSets[pContext->currentCbIndex], 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->softwarePipeline);
	vkCmdDispatch(commandBuffer, VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS / 32, VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS / 32, 1);

	// Transfer images to shader readable layout
	barrier = GetImageBarrier(&pContext->colorImages[pContext->currentCbIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
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
	pContext = new RenderContext{};
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

	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
		descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
		descriptorSetAllocInfo.descriptorSetCount = 1;
		descriptorSetAllocInfo.pSetLayouts = &pContext->graphicsDescriptorSetLayout;

		vkAllocateDescriptorSets(pContext->device, &descriptorSetAllocInfo, &pContext->graphicsDescriptorSets[i]);
	}

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

	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		AllocateBuffer(sizeof(ScanlineData) * SCANLINE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pContext->scanlineBuffers[i]);
		CreateImage(VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS, VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, pContext->colorImages[i]);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = pContext->colorImages[i].view;
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

	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		VkDescriptorSetAllocateInfo computeDescriptorSetAllocInfo{};
		computeDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		computeDescriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
		computeDescriptorSetAllocInfo.descriptorSetCount = 1;
		computeDescriptorSetAllocInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;

		vkAllocateDescriptorSets(pContext->device, &computeDescriptorSetAllocInfo, &pContext->computeDescriptorSets[i]);
	}

	// Compute evaluate
	VkPipelineLayoutCreateInfo evaluatePipelineLayoutInfo{};
	evaluatePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	evaluatePipelineLayoutInfo.setLayoutCount = 1;
	evaluatePipelineLayoutInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;

	vkCreatePipelineLayout(pContext->device, &evaluatePipelineLayoutInfo, nullptr, &pContext->evaluatePipelineLayout);

	pContext->evaluateShaderModule = CreateShaderModule("shaders/scanline_evaluate.spv");

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

	pContext->softwareShaderModule = CreateShaderModule("shaders/software.spv");

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

	for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		VkDescriptorImageInfo outBufferInfo{};
		outBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		outBufferInfo.imageView = pContext->colorImages[i].view;
		outBufferInfo.sampler = pContext->defaultSampler;

		VkDescriptorImageInfo paletteBufferInfo{};
		paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		paletteBufferInfo.imageView = pContext->paletteImage.view;
		paletteBufferInfo.sampler = pContext->paletteSampler;

		VkDescriptorBufferInfo chrBufferInfo{};
		chrBufferInfo.buffer = pContext->computeBufferDevice[i].buffer;
		chrBufferInfo.offset = pContext->chrOffset;
		chrBufferInfo.range = pContext->chrSize;

		VkDescriptorBufferInfo palTableInfo{};
		palTableInfo.buffer = pContext->computeBufferDevice[i].buffer;
		palTableInfo.offset = pContext->paletteTableOffset;
		palTableInfo.range = pContext->paletteTableSize;

		VkDescriptorBufferInfo nametableInfo{};
		nametableInfo.buffer = pContext->computeBufferDevice[i].buffer;
		nametableInfo.offset = pContext->nametableOffset;
		nametableInfo.range = pContext->nametableSize;

		VkDescriptorBufferInfo oamInfo{};
		oamInfo.buffer = pContext->computeBufferDevice[i].buffer;
		oamInfo.offset = pContext->oamOffset;
		oamInfo.range = pContext->oamSize;

		VkDescriptorBufferInfo scanlineInfo{};
		scanlineInfo.buffer = pContext->scanlineBuffers[i].buffer;
		scanlineInfo.offset = 0;
		scanlineInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo renderStateInfo{};
		renderStateInfo.buffer = pContext->computeBufferDevice[i].buffer;
		renderStateInfo.offset = pContext->renderStateOffset;
		renderStateInfo.range = pContext->renderStateSize;

		VkWriteDescriptorSet descriptorWrite[8]{};
		descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[0].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[0].dstBinding = 0;
		descriptorWrite[0].dstArrayElement = 0;
		descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite[0].descriptorCount = 1;
		descriptorWrite[0].pImageInfo = &outBufferInfo;

		descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[1].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[1].dstBinding = 1;
		descriptorWrite[1].dstArrayElement = 0;
		descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite[1].descriptorCount = 1;
		descriptorWrite[1].pImageInfo = &paletteBufferInfo;

		descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[2].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[2].dstBinding = 2;
		descriptorWrite[2].dstArrayElement = 0;
		descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[2].descriptorCount = 1;
		descriptorWrite[2].pBufferInfo = &chrBufferInfo;

		descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[3].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[3].dstBinding = 3;
		descriptorWrite[3].dstArrayElement = 0;
		descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[3].descriptorCount = 1;
		descriptorWrite[3].pBufferInfo = &palTableInfo;

		descriptorWrite[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[4].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[4].dstBinding = 4;
		descriptorWrite[4].dstArrayElement = 0;
		descriptorWrite[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[4].descriptorCount = 1;
		descriptorWrite[4].pBufferInfo = &nametableInfo;

		descriptorWrite[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[5].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[5].dstBinding = 5;
		descriptorWrite[5].dstArrayElement = 0;
		descriptorWrite[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[5].descriptorCount = 1;
		descriptorWrite[5].pBufferInfo = &oamInfo;

		descriptorWrite[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[6].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[6].dstBinding = 6;
		descriptorWrite[6].dstArrayElement = 0;
		descriptorWrite[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[6].descriptorCount = 1;
		descriptorWrite[6].pBufferInfo = &scanlineInfo;

		descriptorWrite[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[7].dstSet = pContext->computeDescriptorSets[i];
		descriptorWrite[7].dstBinding = 7;
		descriptorWrite[7].dstArrayElement = 0;
		descriptorWrite[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[7].descriptorCount = 1;
		descriptorWrite[7].pBufferInfo = &renderStateInfo;

		vkUpdateDescriptorSets(pContext->device, 8, descriptorWrite, 0, nullptr);
	}

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
	FreeImage(pContext->paletteImage);
	vkDestroySampler(pContext->device, pContext->paletteSampler, nullptr);

	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		FreeImage(pContext->colorImages[i]);
		FreeBuffer(pContext->computeBufferDevice[i]);
		FreeBuffer(pContext->computeStagingBuffers[i]);
		FreeBuffer(pContext->scanlineBuffers[i]);
	}

	vkDestroyDevice(pContext->device, nullptr);
	vkDestroySurfaceKHR(pContext->instance, pContext->surface, nullptr);
	vkDestroyInstance(pContext->instance, nullptr);

	free(pContext->renderData);
}

void Rendering::DestroyContext() {
	if (pContext == nullptr) {
		return;
	}

	delete pContext;
	pContext = nullptr;
}

//////////////////////////////////////////////////////

void Rendering::BeginFrame() {
	BeginDraw();
	CopyRenderData();
	TransferComputeBufferData();
	RunSoftwareRenderer();
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
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

static void CreateChrPipeline() {
	VkPipelineLayoutCreateInfo chrPipelineLayoutInfo{};
	chrPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	chrPipelineLayoutInfo.setLayoutCount = 1;
	chrPipelineLayoutInfo.pSetLayouts = &pContext->debugDescriptorSetLayout;

	vkCreatePipelineLayout(pContext->device, &chrPipelineLayoutInfo, nullptr, &pContext->chrPipelineLayout);

	pContext->chrShaderModule = CreateShaderModule("shaders/debug_blit_chr.spv");

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

static void CreatePalettePipeline() {
	VkPipelineLayoutCreateInfo chrPipelineLayoutInfo{};
	chrPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	chrPipelineLayoutInfo.setLayoutCount = 1;
	chrPipelineLayoutInfo.pSetLayouts = &pContext->debugDescriptorSetLayout;

	VkResult err = vkCreatePipelineLayout(pContext->device, &chrPipelineLayoutInfo, nullptr, &pContext->palettePipelineLayout);

	pContext->paletteShaderModule = CreateShaderModule("shaders/debug_blit_pal.spv");

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

	err = vkCreateComputePipelines(pContext->device, VK_NULL_HANDLE, 1, &chrCreateInfo, nullptr, &pContext->palettePipeline);
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

	vkCreateDescriptorSetLayout(pContext->device, &computeLayoutInfo, nullptr, &pContext->debugDescriptorSetLayout);

	// TODO: Should these be destroyed?
	CreateChrPipeline();
	CreatePalettePipeline();
}

static void FreeGlobalEditorData() {
	vkDestroyPipeline(pContext->device, pContext->chrPipeline, nullptr);
	vkDestroyPipelineLayout(pContext->device, pContext->chrPipelineLayout, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->chrShaderModule, nullptr);

	vkDestroyPipeline(pContext->device, pContext->palettePipeline, nullptr);
	vkDestroyPipelineLayout(pContext->device, pContext->palettePipelineLayout, nullptr);
	vkDestroyShaderModule(pContext->device, pContext->paletteShaderModule, nullptr);

	vkDestroyDescriptorSetLayout(pContext->device, pContext->debugDescriptorSetLayout, nullptr);
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

	vkCreateDescriptorPool(pContext->device, &poolInfo, nullptr, &pContext->imGuiDescriptorPool);

	ImGui_ImplVulkan_InitInfo vulkanInitInfo{};
	vulkanInitInfo.Instance = pContext->instance;
	vulkanInitInfo.PhysicalDevice = pContext->physicalDevice;
	vulkanInitInfo.Device = pContext->device;
	vulkanInitInfo.QueueFamily = pContext->primaryQueueFamilyIndex;
	vulkanInitInfo.Queue = pContext->primaryQueue;
	vulkanInitInfo.DescriptorPool = pContext->imGuiDescriptorPool;
	vulkanInitInfo.RenderPass = pContext->renderImagePass;
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
};

EditorRenderBuffer* Rendering::CreateEditorBuffer(u32 size, const void* data) {
	EditorRenderBuffer* pBuffer = (EditorRenderBuffer*)calloc(1, sizeof(EditorRenderBuffer));
	if (!pBuffer) {
		DEBUG_ERROR("Failed to allocate editor buffer!\n");
		return nullptr;
	}

	AllocateBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pBuffer->buffer);
	if (data) {
		UpdateEditorBuffer(pBuffer, data);
	}

	return pBuffer;
}
bool Rendering::UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data) {
	if (!pBuffer || !data) {
		return false;
	}

	vkWaitForFences(pContext->device, COMMAND_BUFFER_COUNT, pContext->commandBufferFences, VK_TRUE, UINT64_MAX);
	// TODO: Error check
	void* mappedData;
	vkMapMemory(pContext->device, pBuffer->buffer.memory, 0, pBuffer->buffer.size, 0, &mappedData);
	memcpy(mappedData, data, pBuffer->buffer.size);
	vkUnmapMemory(pContext->device, pBuffer->buffer.memory);

	return true;
}
void Rendering::FreeEditorBuffer(EditorRenderBuffer* pBuffer) {
	if (!pBuffer) {
		return;
	}

	vkWaitForFences(pContext->device, COMMAND_BUFFER_COUNT, pContext->commandBufferFences, VK_TRUE, UINT64_MAX);
	FreeBuffer(pBuffer->buffer);
	free(pBuffer);
}

static void UpdateEditorSrcDescriptorSet(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer = nullptr, const EditorRenderBuffer* pPaletteBuffer = nullptr) {
	VkDescriptorImageInfo outBufferInfo{};
	outBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	outBufferInfo.imageView = pTexture->image.view;
	outBufferInfo.sampler = pContext->defaultSampler;

	VkDescriptorImageInfo paletteBufferInfo{};
	paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	paletteBufferInfo.imageView = pContext->paletteImage.view;
	paletteBufferInfo.sampler = pContext->paletteSampler;

	VkDescriptorBufferInfo chrBufferInfo{};
	chrBufferInfo.buffer = pContext->computeBufferDevice[pContext->currentCbIndex].buffer;
	chrBufferInfo.offset = pContext->chrOffset;
	chrBufferInfo.range = pContext->chrSize;

	VkDescriptorBufferInfo palTableInfo{};
	palTableInfo.buffer = pContext->computeBufferDevice[pContext->currentCbIndex].buffer;
	palTableInfo.offset = pContext->paletteTableOffset;
	palTableInfo.range = pContext->paletteTableSize;

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

	vkUpdateDescriptorSets(pContext->device, 4, descriptorWrite, 0, nullptr);
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

	vkCmdBlitImage(temp, pContext->paletteImage.image, VK_IMAGE_LAYOUT_GENERAL, pTexture->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, rowCount, regions, VK_FILTER_NEAREST);

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

	vkQueueSubmit(pContext->primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(pContext->primaryQueue);

	vkFreeCommandBuffers(pContext->device, pContext->primaryCommandPool, 1, &temp);
}

EditorRenderTexture* Rendering::CreateEditorTexture(u32 width, u32 height, u32 usage, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
	EditorRenderTexture* pTexture = (EditorRenderTexture*)calloc(1, sizeof(EditorRenderTexture));
	if (!pTexture) {
		DEBUG_ERROR("Failed to allocate editor texture!\n");
		return nullptr;
	}

	pTexture->usage = usage;

	CreateImage(width, height, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, pTexture->image);
	pTexture->dstSet = ImGui_ImplVulkan_AddTexture(pContext->defaultSampler, pTexture->image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	if (usage == EDITOR_TEXTURE_USAGE_COLOR) {
		BlitEditorColorsTexture(pTexture);
	}
	else {
		VkDescriptorSetAllocateInfo chrDescriptorSetAllocInfo{};
		chrDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		chrDescriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
		chrDescriptorSetAllocInfo.descriptorSetCount = 1;
		chrDescriptorSetAllocInfo.pSetLayouts = &pContext->debugDescriptorSetLayout;

		vkAllocateDescriptorSets(pContext->device, &chrDescriptorSetAllocInfo, &pTexture->srcSet);
		UpdateEditorSrcDescriptorSet(pTexture, pChrBuffer, pPaletteBuffer);
	}

	return pTexture;
}
bool Rendering::UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
	if (!pTexture) {
		return false;
	}

	if (pTexture->usage == EDITOR_TEXTURE_USAGE_COLOR) {
		DEBUG_ERROR("Cannot update color texture!\n");
		return false;
	}

	vkWaitForFences(pContext->device, COMMAND_BUFFER_COUNT, pContext->commandBufferFences, VK_TRUE, UINT64_MAX);
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

	vkWaitForFences(pContext->device, COMMAND_BUFFER_COUNT, pContext->commandBufferFences, VK_TRUE, UINT64_MAX);
	ImGui_ImplVulkan_RemoveTexture(pTexture->dstSet);
	FreeImage(pTexture->image);
	// NOTE: Requires VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to free individual descriptor sets
	//vkFreeDescriptorSets(pContext->device, pContext->descriptorPool, 1, &pTexture->srcSet);
	free(pTexture);
}

void Rendering::RenderEditorTexture(const EditorRenderTexture* pTexture) {
	if (!pTexture) {
		return;
	}

	if (pTexture->usage == EDITOR_TEXTURE_USAGE_COLOR) {
		DEBUG_ERROR("Cannot render color texture!\n");
		return;
	}

	VkCommandBuffer cmd = pContext->primaryCommandBuffers[pContext->currentCbIndex];

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
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->chrPipelineLayout, 0, 1, &pTexture->srcSet, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->chrPipeline);
		vkCmdDispatch(cmd, pTexture->image.width / TILE_DIM_PIXELS, pTexture->image.height / TILE_DIM_PIXELS, 1);
	}
	else if (pTexture->usage == EDITOR_TEXTURE_USAGE_PALETTE) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->palettePipelineLayout, 0, 1, &pTexture->srcSet, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->palettePipeline);
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
	VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData != nullptr) {
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	}
}
#endif