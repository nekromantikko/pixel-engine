#ifdef PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <cstring>
#include "rendering.h"
#include "system.h"

namespace Rendering
{
	struct RIFFHeader {
		const char signature[4]; // Should be 'RIFF'
		u32 size;
		const char type[4];
	};
	struct PaletteChunkHeader {
		char signature[4];
		u32 size;
	};

	struct Tile {
		u8 pixels[TILE_SIZE* TILE_SIZE];
	};

	struct ScanlineData {
		u32 spriteCount;
		u32 spriteIndices[MAX_SPRITES_PER_SCANLINE];

		s32 scrollX;
		s32 scrollY;
		u32 bgChrIndex;
		u32 fgChrIndex;
	};

	struct CommandBuffer {
		VkCommandBuffer cmd;
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

		u32 currentCbIndex;
#define COMMAND_BUFFER_COUNT 2
		VkCommandBuffer primaryCommandBuffers[COMMAND_BUFFER_COUNT];
		VkFence commandBufferFences[COMMAND_BUFFER_COUNT];
		VkSemaphore imageAcquiredSemaphores[COMMAND_BUFFER_COUNT];
		VkSemaphore drawCompleteSemaphores[COMMAND_BUFFER_COUNT];

		VkSwapchainKHR swapchain;
		u32 currentSwaphainIndex;
#define SWAPCHAIN_IMAGE_COUNT 3
		VkImage swapchainImages[SWAPCHAIN_IMAGE_COUNT];
		VkImageView swapchainImageViews[SWAPCHAIN_IMAGE_COUNT];
		VkFramebuffer swapchainFramebuffers[SWAPCHAIN_IMAGE_COUNT];

		// General stuff
		VkBuffer timeBuffer;
		VkDeviceMemory timeMemory;

		// Grafix
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;
		VkDescriptorSetLayout graphicsDescriptorSetLayout;
		VkDescriptorSet graphicsDescriptorSets[COMMAND_BUFFER_COUNT];
		VkRenderPass renderImagePass;
		VkPipelineLayout graphicsPipelineLayout;
		VkPipeline graphicsPipeline;

		// Compute stuff
		VkImage paletteImage;
		VkImageView paletteImageView;
		VkSampler paletteSampler;
		VkDeviceMemory paletteMemory;

		VkBuffer scanlineBuffer;
		VkDeviceMemory scanlineMemory;

		// 8 palettes of 8 colors
		VkBuffer palTableBuffer;
		VkDeviceMemory palTableMemory;

		// Pattern table
		VkBuffer chrBuffer;
		VkDeviceMemory chrMemory;
		VkDeviceSize chrSize;
		// Nametable
		VkBuffer nametableBuffer;
		VkDeviceMemory nametableMemory;

		// OAM
		VkBuffer oamBuffer;
		VkDeviceMemory oamMemory;

		VkImage colorImage;
		VkImageView colorImageView;
		VkSampler colorSampler;
		VkDeviceMemory colorImageMemory;

		VkDescriptorSetLayout computeDescriptorSetLayout;
		VkDescriptorSet computeDescriptorSet;
		VkPipelineLayout computePipelineLayout;
		VkPipeline computePipeline;
		VkShaderModule noiseShaderModule;
		VkPipelineLayout evaluatePipelineLayout;
		VkPipeline evaluatePipeline;
		VkShaderModule evaluateShaderModule;

		// Render state from game code...
		VkBuffer renderStateBuffer;
		VkDeviceMemory renderStateMemory;
	};

	void CreateVulkanInstance(RenderContext *pContext) {
		VkApplicationInfo appInfo;
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "";
		appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
		appInfo.pEngineName = "";
		appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		const char* debugLayerName = "VK_LAYER_KHRONOS_validation";

		const char* extensionNames[2];
		u32 extensionCount = 0;
		extensionNames[extensionCount++] = "VK_KHR_surface";
#ifdef VK_USE_PLATFORM_WIN32_KHR
		extensionNames[extensionCount++] = "VK_KHR_win32_surface";
#endif

		VkInstanceCreateInfo createInfo;
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
			ERROR("Failed to create instance!\n");
		}
	}

	bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, u32 &outQueueFamilyIndex) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

		u32 extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
		VkExtensionProperties* availableExtensions = (VkExtensionProperties *)malloc(extensionCount * sizeof(VkExtensionProperties));
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
		VkSurfaceFormatKHR* availableFormats = (VkSurfaceFormatKHR *)malloc(surfaceFormatCount * sizeof(VkSurfaceFormatKHR));
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableFormats);

		u32 presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
		VkPresentModeKHR* availablePresentModes = (VkPresentModeKHR*)malloc(presentModeCount * sizeof(VkPresentModeKHR));
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, availablePresentModes);

		// TODO: Something with these
		free(availableFormats);
		free(availablePresentModes);

		if (surfaceFormatCount == 0 || presentModeCount == 0) {
			return false;
		}

		u32 queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
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

	void GetSuitablePhysicalDevice(RenderContext* pContext) {
		u32 physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(pContext->instance, &physicalDeviceCount, nullptr);
		if (physicalDeviceCount == 0) {
			ERROR("No devices found for some reason!\n");
		}
		VkPhysicalDevice *availableDevices = (VkPhysicalDevice *)malloc(physicalDeviceCount * sizeof(VkPhysicalDevice));
		vkEnumeratePhysicalDevices(pContext->instance, &physicalDeviceCount, availableDevices);

		bool physicalDeviceFound = false;
		VkPhysicalDevice foundDevice = VK_NULL_HANDLE;
		u32 foundQueueFamilyIndex = 0;

		for (u32 i = 0; i < physicalDeviceCount; i++) {
			u32 queueFamilyIndex;
			if (IsPhysicalDeviceSuitable(availableDevices[i], pContext->surface, queueFamilyIndex)) {
				physicalDeviceFound = true;
				foundDevice = availableDevices[i];
				foundQueueFamilyIndex = queueFamilyIndex;
			}
		}

		free(availableDevices);

		if (!physicalDeviceFound) {
			ERROR("No suitable physical device found!\n");
		}
		pContext->primaryQueueFamilyIndex = foundQueueFamilyIndex;
		pContext->physicalDevice = foundDevice;
	}

	void CreateDevice(RenderContext* pContext) {
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo queueCreateInfo;
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
			ERROR("Failed to create logical device!\n");
		}
	}

	void CreateSwapchain(RenderContext* pContext) {
		if (SWAPCHAIN_IMAGE_COUNT > pContext->surfaceCapabilities.maxImageCount || SWAPCHAIN_IMAGE_COUNT < pContext->surfaceCapabilities.minImageCount) {
			ERROR("Image count not supported!\n");
		}

		if (pContext->renderImagePass == VK_NULL_HANDLE) {
			ERROR("Invalid render pass!\n");
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
			ERROR("Failed to create swapchain!\n");
		}

		u32 imageCount = 0;
		vkGetSwapchainImagesKHR(pContext->device, pContext->swapchain, &imageCount, nullptr);
		if (imageCount != SWAPCHAIN_IMAGE_COUNT) {
			ERROR("Something very weird happened\n");
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
				ERROR("Failed to create image view!\n");
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
				ERROR("Failed to create framebuffer!\n");
			}
		}
	}

	void FreeSwapchain(RenderContext* pContext) {
		for (u32 i = 0; i < SWAPCHAIN_IMAGE_COUNT; i++) {
			vkDestroyFramebuffer(pContext->device, pContext->swapchainFramebuffers[i], nullptr);
			vkDestroyImageView(pContext->device, pContext->swapchainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(pContext->device, pContext->swapchain, nullptr);
	}

	VkShaderModule CreateShaderModule(VkDevice device, const char* code, const u32 size) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = size;
		createInfo.pCode = (const u32*)code;

		VkShaderModule module;
		VkResult err = vkCreateShaderModule(device, &createInfo, nullptr, &module);
		if (err != VK_SUCCESS) {
			ERROR("Failed to create shader module!\n");
		}
		return module;
	}

	// TODO: Absorb into other function
	void CreateRenderPass(RenderContext* pContext) {
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
			ERROR("failed to create render pass!");
		}
	}

	u32 GetDeviceMemoryTypeIndex(RenderContext *pContext, u32 typeFilter, VkMemoryPropertyFlags propertyFlags) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(pContext->physicalDevice, &memProperties);

		for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
				return i;
			}
		}
	}

	VkCommandBuffer GetTemporaryCommandBuffer(RenderContext* pContext) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = pContext->primaryCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(pContext->device, &allocInfo, &commandBuffer);

		return commandBuffer;
	}

	void CreateGraphicsPipeline(RenderContext* pContext)
	{
		u32 vertShaderLength;
		char* vertShader = AllocFileBytes("test_vert.spv", vertShaderLength);
		u32 fragShaderLength;
		char* fragShader = AllocFileBytes("test_frag.spv", fragShaderLength);
		DEBUG_LOG("Vert shader length: %d\n", vertShaderLength);
		DEBUG_LOG("Frag shader length: %d\n", fragShaderLength);
		pContext->vertShaderModule = CreateShaderModule(pContext->device, vertShader, vertShaderLength);
		pContext->fragShaderModule = CreateShaderModule(pContext->device, fragShader, fragShaderLength);
		free(vertShader);
		free(fragShader);
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = pContext->vertShaderModule;
		vertShaderStageInfo.pName = "main";
		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = pContext->fragShaderModule;
		fragShaderStageInfo.pName = "main";
		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

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

		VkPipelineLayoutCreateInfo graphicsPipelineLayoutInfo{};
		graphicsPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		graphicsPipelineLayoutInfo.setLayoutCount = 1;
		graphicsPipelineLayoutInfo.pSetLayouts = &pContext->graphicsDescriptorSetLayout;
		graphicsPipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		graphicsPipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		if (vkCreatePipelineLayout(pContext->device, &graphicsPipelineLayoutInfo, nullptr, &pContext->graphicsPipelineLayout) != VK_SUCCESS) {
			ERROR("failed to create pipeline layout!");
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

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.layout = pContext->graphicsPipelineLayout;
		pipelineInfo.renderPass = pContext->renderImagePass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		VkResult err = vkCreateGraphicsPipelines(pContext->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pContext->graphicsPipeline);
		if (err != VK_SUCCESS) {
			ERROR("failed to create graphics pipeline!");
		}
	}

	void FreeGraphicsPipeline(RenderContext* pRenderContext)
	{
		vkDestroyPipeline(pRenderContext->device, pRenderContext->graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(pRenderContext->device, pRenderContext->graphicsPipelineLayout, nullptr);
		vkDestroyRenderPass(pRenderContext->device, pRenderContext->renderImagePass, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->vertShaderModule, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->fragShaderModule, nullptr);
		vkDestroyDescriptorSetLayout(pRenderContext->device, pRenderContext->graphicsDescriptorSetLayout, nullptr);
	}

	void AllocateMemory(RenderContext* pContext, VkMemoryRequirements requirements, VkMemoryPropertyFlags properties, VkDeviceMemory& outMemory) {
		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.allocationSize = requirements.size;
		memAllocInfo.memoryTypeIndex = GetDeviceMemoryTypeIndex(pContext, requirements.memoryTypeBits, properties);

		VkResult err = vkAllocateMemory(pContext->device, &memAllocInfo, nullptr, &outMemory);
		if (err != VK_SUCCESS) {
			ERROR("Failed to allocate memory!\n");
		}
	}

	void AllocateBuffer(RenderContext* pContext, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, VkBuffer& outBuffer, VkDeviceMemory& outMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;
		bufferInfo.flags = 0;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkResult err = vkCreateBuffer(pContext->device, &bufferInfo, nullptr, &outBuffer);
		if (err != VK_SUCCESS) {
			ERROR("Failed to create buffer!\n");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(pContext->device, outBuffer, &memRequirements);
		DEBUG_LOG("Buffer memory required: %d\n", memRequirements.size);

		AllocateMemory(pContext, memRequirements, memProps, outMemory);

		vkBindBufferMemory(pContext->device, outBuffer, outMemory, 0);
	}

	void CopyBuffer(RenderContext* pContext, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

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

	void CreateOAM(RenderContext* pContext) {
		u32 oamSize = MAX_SPRITE_COUNT * sizeof(Sprite);
		AllocateBuffer(pContext, oamSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pContext->oamBuffer, pContext->oamMemory);
	}

	void CreateNametable(RenderContext* pContext) {
		DEBUG_LOG("Creating nametables...\n");
		AllocateBuffer(pContext, NAMETABLE_SIZE * NAMETABLE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pContext->nametableBuffer, pContext->nametableMemory);
	}

	void CreateChrSheet(RenderContext* pContext, u8 index, const char *fname) {
		u32 imgWidth, imgHeight;
		u16 bpp;
		char* imgData = LoadBitmapBytes(fname, imgWidth, imgHeight, bpp);

		if (imgWidth != 128 || imgHeight != 128) {
			ERROR("Invalid chr image dimensions!\n");
		}

		if (bpp != 8) {
			ERROR("Invalid chr image format!\n");
		}
		
		void* data;
		vkMapMemory(pContext->device, pContext->chrMemory, CHR_SHEET_SIZE*index, CHR_SHEET_SIZE, 0, &data);
		
		Tile* tileData = (Tile*)data;
		for (u32 y = 0; y < imgHeight; y++) {
			for (u32 x = 0; x < imgWidth; x++) {
				u32 coarseX = x / 8;
				u32 coarseY = y / 8;
				u32 fineX = x % 8;
				u32 fineY = y % 8;
				u32 tileIndex = (15 - coarseY) * 16 + coarseX; // Tile 0 is top left instead of bottom left
				u32 inPixelIndex = y * imgWidth + x;
				u32 outPixelIndex = (7 - fineY) * 8 + fineX; // Also pixels go from top to bottom in this program, but bottom to top in bmp, so flip
				tileData[tileIndex].pixels[outPixelIndex] = imgData[inPixelIndex];
			}
		}

		vkUnmapMemory(pContext->device, pContext->chrMemory);
		free(imgData);

		pContext->chrSize = CHR_SHEET_SIZE;
	}

	void CreatePaletteTable(RenderContext* pContext) {
		u32 palFileSize;
		char* palData = AllocFileBytes("palette.dat", palFileSize);

		if (palFileSize < 8 * 8) {
			ERROR("Invalid palette table file!\n");
		}

		AllocateBuffer(pContext, 8 * 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pContext->palTableBuffer, pContext->palTableMemory);

		void* data;
		vkMapMemory(pContext->device, pContext->palTableMemory, 0, 8 * 8, 0, &data);
		memcpy(data, palData, 8 * 8);
		vkUnmapMemory(pContext->device, pContext->palTableMemory);
		free(palData);
	}

	void CreatePalette(RenderContext* pContext) {
		u32 *paletteData = nullptr;
		VkDeviceSize paletteSize = 0;
		u16 colorCount = 0;

		u32 palFileSize;
		char *palBin = AllocFileBytes("famicube.pal", palFileSize);
		DEBUG_LOG("%d\n", palFileSize);
		if (palFileSize < sizeof(RIFFHeader)) {
			ERROR("Invalid palette file!\n");
		}
		RIFFHeader* header = (RIFFHeader*)palBin;
		DEBUG_LOG("%.4s\n", header->signature);
		DEBUG_LOG("%.4s\n", header->type);
		if (strncmp(header->signature, "RIFF", 4) != 0 || strncmp(header->type, "PAL ", 4) != 0 || header->size + 8 != palFileSize){
			ERROR("Invalid palette file!\n");
		}
		char* pos = palBin + sizeof(RIFFHeader);
		char* fileEnd = pos + header->size;
		while (pos + 1 < fileEnd) {
			PaletteChunkHeader* chunkHeader = (PaletteChunkHeader*)pos;
			pos += sizeof(PaletteChunkHeader);
			if (strncmp(chunkHeader->signature, "data", 4) == 0) {
				u16* palVer = (u16*)pos;
				DEBUG_LOG("Palette version = 0x%x\n", *palVer);
				colorCount = *(palVer + 1);
				DEBUG_LOG("Color count = %d\n", colorCount);
				paletteData = ((u32*)pos) + 1;
				paletteSize = colorCount * sizeof(u32);
				break;
			}
			pos += chunkHeader->size;
		}

		DEBUG_LOG("0x%x\n", paletteData);
		if (paletteData == nullptr || colorCount == 0) {
			ERROR("No palette data found in file!\n");
		}

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		AllocateBuffer(pContext, paletteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

		void* data;
		vkMapMemory(pContext->device, stagingMemory, 0, paletteSize, 0, &data);
		memcpy(data, paletteData, paletteSize);
		vkUnmapMemory(pContext->device, stagingMemory);
		free(palBin);

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_1D;
		imageInfo.extent.width = 64;
		imageInfo.extent.height = 1;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		vkCreateImage(pContext->device, &imageInfo, nullptr, &pContext->paletteImage);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(pContext->device, pContext->paletteImage, &memRequirements);

		AllocateMemory(pContext, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pContext->paletteMemory);
		vkBindImageMemory(pContext->device, pContext->paletteImage, pContext->paletteMemory, 0);

		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(temp, &beginInfo);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = pContext->paletteImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0; // TODO
		barrier.dstAccessMask = 0; // TODO

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
			64,
			1,
			1
		};

		vkCmdCopyBufferToImage(
			temp,
			stagingBuffer,
			pContext->paletteImage,
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

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = pContext->paletteImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		vkCreateImageView(pContext->device, &viewInfo, nullptr, &pContext->paletteImageView);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
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

		vkCreateSampler(pContext->device, &samplerInfo, nullptr, &pContext->paletteSampler);

		vkDestroyBuffer(pContext->device, stagingBuffer, nullptr);
		vkFreeMemory(pContext->device, stagingMemory, nullptr);
	}

	RenderContext *CreateRenderContext(Surface surface) {
		RenderContext *context = (RenderContext*)malloc(sizeof(RenderContext));
		if (context == nullptr) {
			ERROR("Couldn't allocate memory for renderContext\n");
		}

		CreateVulkanInstance(context);

		context->surface = VK_NULL_HANDLE;
#ifdef VK_USE_PLATFORM_WIN32_KHR
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.hwnd = surface.hWnd;
		surfaceCreateInfo.hinstance = surface.hInstance;

		vkCreateWin32SurfaceKHR(context->instance, &surfaceCreateInfo, nullptr, &context->surface);
#endif

		GetSuitablePhysicalDevice(context);
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physicalDevice, context->surface, &context->surfaceCapabilities);
		CreateDevice(context);
		vkGetDeviceQueue(context->device, context->primaryQueueFamilyIndex, 0, &context->primaryQueue);
		CreateRenderPass(context);
		CreateSwapchain(context);

		VkDescriptorPoolSize poolSizes[3]{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = COMMAND_BUFFER_COUNT;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = COMMAND_BUFFER_COUNT;
		poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[2].descriptorCount = 2;

		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = 3;
		descriptorPoolInfo.pPoolSizes = poolSizes;
		descriptorPoolInfo.maxSets = COMMAND_BUFFER_COUNT + 1;

		vkCreateDescriptorPool(context->device, &descriptorPoolInfo, nullptr, &context->descriptorPool);

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

		vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, &context->graphicsDescriptorSetLayout);

		VkDescriptorSetLayout layouts[COMMAND_BUFFER_COUNT];
		for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
			layouts[i] = context->graphicsDescriptorSetLayout;
		}
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
		descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocInfo.descriptorPool = context->descriptorPool;
		descriptorSetAllocInfo.descriptorSetCount = COMMAND_BUFFER_COUNT;
		descriptorSetAllocInfo.pSetLayouts = layouts;

		vkAllocateDescriptorSets(context->device, &descriptorSetAllocInfo, context->graphicsDescriptorSets);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = context->primaryQueueFamilyIndex;

		if (vkCreateCommandPool(context->device, &poolInfo, nullptr, &context->primaryCommandPool) != VK_SUCCESS) {
			ERROR("failed to create command pool!");
		}

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = context->primaryCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = COMMAND_BUFFER_COUNT;

		if (vkAllocateCommandBuffers(context->device, &allocInfo, context->primaryCommandBuffers) != VK_SUCCESS) {
			ERROR("failed to allocate command buffers!");
		}

		context->currentCbIndex = 0;

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
			vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &context->imageAcquiredSemaphores[i]);
			vkCreateSemaphore(context->device, &semaphoreInfo, nullptr, &context->drawCompleteSemaphores[i]);
			vkCreateFence(context->device, &fenceInfo, nullptr, &context->commandBufferFences[i]);
		}

		CreateGraphicsPipeline(context);

		// Compute resources
		CreatePalette(context);
		CreatePaletteTable(context);
		AllocateBuffer(context, CHR_SHEET_SIZE * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, context->chrBuffer, context->chrMemory);
		CreateChrSheet(context, 0, "CHR000.bmp");
		CreateChrSheet(context, 1, "CHR001.bmp");
		CreateNametable(context);
		CreateOAM(context);
		ClearSprites(context, 0, MAX_SPRITE_COUNT);
		AllocateBuffer(context, sizeof(ScanlineData) * SCANLINE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context->scanlineBuffer, context->scanlineMemory);
		AllocateBuffer(context, sizeof(RenderState) * SCANLINE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, context->renderStateBuffer, context->renderStateMemory);
		AllocateBuffer(context, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, context->timeBuffer, context->timeMemory);

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = 512;
		imageInfo.extent.height = 288;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		vkCreateImage(context->device, &imageInfo, nullptr, &context->colorImage);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(context->device, context->colorImage, &memRequirements);
		AllocateMemory(context, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context->colorImageMemory);
		vkBindImageMemory(context->device, context->colorImage, context->colorImageMemory, 0);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = context->colorImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		vkCreateImageView(context->device, &viewInfo, nullptr, &context->colorImageView);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
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

		vkCreateSampler(context->device, &samplerInfo, nullptr, &context->colorSampler);

		// Write into descriptor sets...
		for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = context->colorImageView;
			imageInfo.sampler = context->colorSampler;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = context->graphicsDescriptorSets[i];
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(context->device, 1, &descriptorWrite, 0, nullptr);
		}

		// COMPUTE

		//setup push constants
		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(RenderState);
		pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

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

		vkCreateDescriptorSetLayout(context->device, &computeLayoutInfo, nullptr, &context->computeDescriptorSetLayout);

		VkDescriptorSetAllocateInfo computeDescriptorSetAllocInfo{};
		computeDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		computeDescriptorSetAllocInfo.descriptorPool = context->descriptorPool;
		computeDescriptorSetAllocInfo.descriptorSetCount = 1;
		computeDescriptorSetAllocInfo.pSetLayouts = &context->computeDescriptorSetLayout;

		vkAllocateDescriptorSets(context->device, &computeDescriptorSetAllocInfo, &context->computeDescriptorSet);

		// Compute evaluate
		VkPipelineLayoutCreateInfo evaluatePipelineLayoutInfo{};
		evaluatePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		evaluatePipelineLayoutInfo.setLayoutCount = 1;
		evaluatePipelineLayoutInfo.pSetLayouts = &context->computeDescriptorSetLayout;

		vkCreatePipelineLayout(context->device, &evaluatePipelineLayoutInfo, nullptr, &context->evaluatePipelineLayout);

		u32 evaluateShaderLength;
		char* evaluateShader = AllocFileBytes("scanline_evaluate_comp.spv", evaluateShaderLength);
		context->evaluateShaderModule = CreateShaderModule(context->device, evaluateShader, evaluateShaderLength);
		free(evaluateShader);

		VkPipelineShaderStageCreateInfo evaluateShaderStageInfo{};
		evaluateShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		evaluateShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		evaluateShaderStageInfo.module = context->evaluateShaderModule;
		evaluateShaderStageInfo.pName = "main";

		VkComputePipelineCreateInfo evaluateCreateInfo{};
		evaluateCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		evaluateCreateInfo.flags = 0;
		evaluateCreateInfo.stage = evaluateShaderStageInfo;
		evaluateCreateInfo.layout = context->evaluatePipelineLayout;

		vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1, &evaluateCreateInfo, nullptr, &context->evaluatePipeline);

		// Compute rendering
		VkPipelineLayoutCreateInfo computePipelineLayoutInfo{};
		computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		computePipelineLayoutInfo.setLayoutCount = 1;
		computePipelineLayoutInfo.pSetLayouts = &context->computeDescriptorSetLayout;
		computePipelineLayoutInfo.pPushConstantRanges = &pushConstant;
		computePipelineLayoutInfo.pushConstantRangeCount = 1;

		vkCreatePipelineLayout(context->device, &computePipelineLayoutInfo, nullptr, &context->computePipelineLayout);

		u32 noiseShaderLength;
		char* noiseShader = AllocFileBytes("white_noise_comp.spv", noiseShaderLength);
		context->noiseShaderModule = CreateShaderModule(context->device, noiseShader, noiseShaderLength);
		free(noiseShader);

		VkPipelineShaderStageCreateInfo compShaderStageInfo{};
		compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compShaderStageInfo.module = context->noiseShaderModule;
		compShaderStageInfo.pName = "main";

		VkComputePipelineCreateInfo computeCreateInfo{};
		computeCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computeCreateInfo.flags = 0;
		computeCreateInfo.stage = compShaderStageInfo;
		computeCreateInfo.layout = context->computePipelineLayout;

		vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, &context->computePipeline);

		VkDescriptorImageInfo perkele{};
		perkele.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		perkele.imageView = context->colorImageView;
		perkele.sampler = context->colorSampler;

		VkDescriptorImageInfo paletteBufferInfo{};
		paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		paletteBufferInfo.imageView = context->paletteImageView;
		paletteBufferInfo.sampler = context->paletteSampler;

		VkDescriptorBufferInfo chrBufferInfo{};
		chrBufferInfo.buffer = context->chrBuffer;
		chrBufferInfo.offset = 0;
		chrBufferInfo.range = context->chrSize * 2;

		VkDescriptorBufferInfo palTableInfo{};
		palTableInfo.buffer = context->palTableBuffer;
		palTableInfo.offset = 0;
		palTableInfo.range = 8 * 8;

		VkDescriptorBufferInfo nametableInfo{};
		nametableInfo.buffer = context->nametableBuffer;
		nametableInfo.offset = 0;
		nametableInfo.range = NAMETABLE_SIZE * NAMETABLE_COUNT;

		VkDescriptorBufferInfo oamInfo{};
		oamInfo.buffer = context->oamBuffer;
		oamInfo.offset = 0;
		oamInfo.range = sizeof(Sprite) * MAX_SPRITE_COUNT;

		VkDescriptorBufferInfo scanlineInfo{};
		scanlineInfo.buffer = context->scanlineBuffer;
		scanlineInfo.offset = 0;
		scanlineInfo.range = sizeof(ScanlineData) * SCANLINE_COUNT;

		VkDescriptorBufferInfo renderStateInfo{};
		renderStateInfo.buffer = context->renderStateBuffer;
		renderStateInfo.offset = 0;
		renderStateInfo.range = sizeof(RenderState) * SCANLINE_COUNT;

		VkWriteDescriptorSet descriptorWrite[8]{};
		descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[0].dstSet = context->computeDescriptorSet;
		descriptorWrite[0].dstBinding = 0;
		descriptorWrite[0].dstArrayElement = 0;
		descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite[0].descriptorCount = 1;
		descriptorWrite[0].pImageInfo = &perkele;

		descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[1].dstSet = context->computeDescriptorSet;
		descriptorWrite[1].dstBinding = 1;
		descriptorWrite[1].dstArrayElement = 0;
		descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite[1].descriptorCount = 1;
		descriptorWrite[1].pImageInfo = &paletteBufferInfo;

		descriptorWrite[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[2].dstSet = context->computeDescriptorSet;
		descriptorWrite[2].dstBinding = 2;
		descriptorWrite[2].dstArrayElement = 0;
		descriptorWrite[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[2].descriptorCount = 1;
		descriptorWrite[2].pBufferInfo = &chrBufferInfo;

		descriptorWrite[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[3].dstSet = context->computeDescriptorSet;
		descriptorWrite[3].dstBinding = 3;
		descriptorWrite[3].dstArrayElement = 0;
		descriptorWrite[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[3].descriptorCount = 1;
		descriptorWrite[3].pBufferInfo = &palTableInfo;

		descriptorWrite[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[4].dstSet = context->computeDescriptorSet;
		descriptorWrite[4].dstBinding = 4;
		descriptorWrite[4].dstArrayElement = 0;
		descriptorWrite[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[4].descriptorCount = 1;
		descriptorWrite[4].pBufferInfo = &nametableInfo;

		descriptorWrite[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[5].dstSet = context->computeDescriptorSet;
		descriptorWrite[5].dstBinding = 5;
		descriptorWrite[5].dstArrayElement = 0;
		descriptorWrite[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[5].descriptorCount = 1;
		descriptorWrite[5].pBufferInfo = &oamInfo;

		descriptorWrite[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[6].dstSet = context->computeDescriptorSet;
		descriptorWrite[6].dstBinding = 6;
		descriptorWrite[6].dstArrayElement = 0;
		descriptorWrite[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[6].descriptorCount = 1;
		descriptorWrite[6].pBufferInfo = &scanlineInfo;

		descriptorWrite[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite[7].dstSet = context->computeDescriptorSet;
		descriptorWrite[7].dstBinding = 7;
		descriptorWrite[7].dstArrayElement = 0;
		descriptorWrite[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrite[7].descriptorCount = 1;
		descriptorWrite[7].pBufferInfo = &renderStateInfo;

		vkUpdateDescriptorSets(context->device, 8, descriptorWrite, 0, nullptr);

		return context;
	}

	void FreeRenderContext(RenderContext* pRenderContext) {
		if (pRenderContext == nullptr) {
			return;
		}

		// Wait for all commands to execute first
		vkWaitForFences(pRenderContext->device, COMMAND_BUFFER_COUNT, pRenderContext->commandBufferFences, VK_TRUE, UINT64_MAX);

		// vkFreeDescriptorSets(pRenderContext->device, pRenderContext->descriptorPool, COMMAND_BUFFER_COUNT, pRenderContext->graphicsDescriptorSets);
		// vkFreeDescriptorSets(pRenderContext->device, pRenderContext->descriptorPool, 1, &pRenderContext->computeDescriptorSet);
		vkDestroyDescriptorPool(pRenderContext->device, pRenderContext->descriptorPool, nullptr);

		for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
			vkDestroySemaphore(pRenderContext->device, pRenderContext->imageAcquiredSemaphores[i], nullptr);
			vkDestroySemaphore(pRenderContext->device, pRenderContext->drawCompleteSemaphores[i], nullptr);
			vkDestroyFence(pRenderContext->device, pRenderContext->commandBufferFences[i], nullptr);
		}

		vkDestroyCommandPool(pRenderContext->device, pRenderContext->primaryCommandPool, nullptr);

		FreeSwapchain(pRenderContext);

		FreeGraphicsPipeline(pRenderContext);

		vkDestroySampler(pRenderContext->device, pRenderContext->colorSampler, nullptr);
		vkDestroySampler(pRenderContext->device, pRenderContext->paletteSampler, nullptr);
		vkDestroyDescriptorSetLayout(pRenderContext->device, pRenderContext->computeDescriptorSetLayout, nullptr);
		vkDestroyPipeline(pRenderContext->device, pRenderContext->computePipeline, nullptr);
		vkDestroyPipelineLayout(pRenderContext->device, pRenderContext->computePipelineLayout, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->noiseShaderModule, nullptr);
		vkDestroyImageView(pRenderContext->device, pRenderContext->colorImageView, nullptr);
		vkDestroyImage(pRenderContext->device, pRenderContext->colorImage, nullptr);
		vkFreeMemory(pRenderContext->device, pRenderContext->colorImageMemory, nullptr);
		vkDestroyImageView(pRenderContext->device, pRenderContext->paletteImageView, nullptr);
		vkDestroyImage(pRenderContext->device, pRenderContext->paletteImage, nullptr);
		vkFreeMemory(pRenderContext->device, pRenderContext->paletteMemory, nullptr);

		vkDestroyDevice(pRenderContext->device, nullptr);
		vkDestroySurfaceKHR(pRenderContext->instance, pRenderContext->surface, nullptr);
		vkDestroyInstance(pRenderContext->instance, nullptr);
		free(pRenderContext);
	}

	//////////////////////////////////////////////////////

	void GetPaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* outColors) {
		if (offset + count > 8 || paletteIndex >= 8) {
			ERROR("Trying to get palette colors outside range!\n");
		}

		u32 actualOffset = 8 * paletteIndex + offset;

		void* data;
		vkMapMemory(pContext->device, pContext->palTableMemory, actualOffset, count, 0, &data);
		memcpy(outColors, data, count);
		vkUnmapMemory(pContext->device, pContext->palTableMemory);
	}
	void SetPaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* colors) {
		if (offset + count > 8 || paletteIndex >= 8) {
			ERROR("Trying to set palette colors outside range!\n");
		}

		u32 actualOffset = 8 * paletteIndex + offset;

		void* data;
		vkMapMemory(pContext->device, pContext->palTableMemory, actualOffset, count, 0, &data);
		memcpy(data, colors, count);
		vkUnmapMemory(pContext->device, pContext->palTableMemory);
	}
	void ClearSprites(RenderContext* pContext, u32 offset, u32 count) {
		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(temp, &beginInfo);

		vkCmdFillBuffer(
			temp,
			pContext->oamBuffer,
			sizeof(Sprite) * offset,
			sizeof(Sprite) * count,
			288 // Set y position offscreen
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
	void GetSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* outSprites) {
		if (offset + count > MAX_SPRITE_COUNT) {
			ERROR("Trying to get sprites outside range!\n");
		}

		u32 actualOffset = offset * sizeof(Sprite);
		u32 size = count * sizeof(Sprite);

		void* data;
		vkMapMemory(pContext->device, pContext->oamMemory, actualOffset, size, 0, &data);
		memcpy((void*)outSprites, data, size);
		vkUnmapMemory(pContext->device, pContext->oamMemory);
	}
	void SetSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* sprites) {
		if (count == 0) {
			return;
		}

		if (offset + count > MAX_SPRITE_COUNT) {
			ERROR("Trying to set spritess outside range!\n");
		}

		u32 actualOffset = offset * sizeof(Sprite);
		u32 size = count * sizeof(Sprite);

		void* data;
		vkMapMemory(pContext->device, pContext->oamMemory, actualOffset, size, 0, &data);
		memcpy(data, (void*)sprites, size);
		vkUnmapMemory(pContext->device, pContext->oamMemory);
	}
	void UpdateNametable(RenderContext* pContext, u16 index, u16 count, u16 offset, u8* tiles) {
		void* data;
		vkMapMemory(pContext->device, pContext->nametableMemory, index * NAMETABLE_SIZE + offset, count, 0, &data);
		memcpy(data, tiles, count);
		vkUnmapMemory(pContext->device, pContext->nametableMemory);
	}

	void SetRenderState(RenderContext* pContext, u32 scanlineOffset, u32 scanlineCount, RenderState state) {
		void* data;
		vkMapMemory(pContext->device, pContext->renderStateMemory, sizeof(RenderState) * scanlineOffset, sizeof(RenderState) * scanlineCount, 0, &data);
		RenderState* scanlineStates = (RenderState*)data;
		for (int i = 0; i < scanlineCount; i++) {
			memcpy(scanlineStates + i, &state, sizeof(RenderState));
		}
		vkUnmapMemory(pContext->device, pContext->renderStateMemory);
	}

	//////////////////////////////////////////////////////

	void SetCurrentTime(RenderContext* pContext, float seconds) {
		void* data;
		vkMapMemory(pContext->device, pContext->timeMemory, 0, sizeof(float), 0, &data);
		memcpy(data, &seconds, sizeof(float));
		vkUnmapMemory(pContext->device, pContext->timeMemory);
	}

	void ExecuteHardcodedCommands(RenderContext *pContext) {
		VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

		// Convert from preinitialized to general
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = pContext->colorImage;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = 0; // TODO
			barrier.dstAccessMask = 0; // TODO

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}

		// Clear old scanline data
		vkCmdFillBuffer(
			commandBuffer,
			pContext->scanlineBuffer,
			0,
			sizeof(ScanlineData) * SCANLINE_COUNT,
			0
		);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->evaluatePipelineLayout, 0, 1, &pContext->computeDescriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->evaluatePipeline);
		vkCmdDispatch(commandBuffer, MAX_SPRITE_COUNT / MAX_SPRITES_PER_SCANLINE, SCANLINE_COUNT / 8, 1);

		VkBufferMemoryBarrier scanlineBarrier{};
		scanlineBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		scanlineBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		scanlineBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		scanlineBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		scanlineBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		scanlineBarrier.buffer = pContext->scanlineBuffer;
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

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->computePipelineLayout, 0, 1, &pContext->computeDescriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->computePipeline);
		vkCmdDispatch(commandBuffer, 16, 9, 1);

		// Convert from preinitialized to general
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = pContext->colorImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0; // TODO
		barrier.dstAccessMask = 0; // TODO

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		// Render to screen
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

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pContext->graphicsPipeline);

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

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pContext->graphicsPipelineLayout, 0, 1, &pContext->graphicsDescriptorSets[pContext->currentCbIndex], 0, nullptr);

		vkCmdDraw(commandBuffer, 4, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			ERROR("failed to record command buffer!");
		}
	}

	void BeginDraw(RenderContext* pRenderContext) {
		// Wait for drawing to finish if it hasn't
		vkWaitForFences(pRenderContext->device, 1, &pRenderContext->commandBufferFences[pRenderContext->currentCbIndex], VK_TRUE, UINT64_MAX);

		// Get next swapchain image index
		VkResult err = vkAcquireNextImageKHR(pRenderContext->device, pRenderContext->swapchain, UINT64_MAX, pRenderContext->imageAcquiredSemaphores[pRenderContext->currentCbIndex], VK_NULL_HANDLE, &pRenderContext->currentSwaphainIndex);
		if (err != VK_SUCCESS) {
		}

		vkResetFences(pRenderContext->device, 1, &pRenderContext->commandBufferFences[pRenderContext->currentCbIndex]);
		vkResetCommandBuffer(pRenderContext->primaryCommandBuffers[pRenderContext->currentCbIndex], 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(pRenderContext->primaryCommandBuffers[pRenderContext->currentCbIndex], &beginInfo) != VK_SUCCESS) {
			ERROR("failed to begin recording command buffer!");
		}

		// Should be ready to draw now!
	}
	
	void EndDraw(RenderContext* pRenderContext) {
		// Submit the above commands
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = { pRenderContext->imageAcquiredSemaphores[pRenderContext->currentCbIndex] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &pRenderContext->primaryCommandBuffers[pRenderContext->currentCbIndex];
		VkSemaphore signalSemaphores[] = { pRenderContext->drawCompleteSemaphores[pRenderContext->currentCbIndex] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		VkResult err = vkQueueSubmit(pRenderContext->primaryQueue, 1, &submitInfo, pRenderContext->commandBufferFences[pRenderContext->currentCbIndex]);

		// Present to swapchain
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapchains[] = { pRenderContext->swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &pRenderContext->currentSwaphainIndex;
		presentInfo.pResults = nullptr; // Optional

		vkQueuePresentKHR(pRenderContext->primaryQueue, &presentInfo);

		// Advance cb index
		pRenderContext->currentCbIndex = (pRenderContext->currentCbIndex + 1) % COMMAND_BUFFER_COUNT;
	}

	void ResizeSurface(RenderContext* pRenderContext, u32 width, u32 height) {
		// Wait for all commands to execute first
		vkWaitForFences(pRenderContext->device, COMMAND_BUFFER_COUNT, pRenderContext->commandBufferFences, VK_TRUE, UINT64_MAX);

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderContext->physicalDevice, pRenderContext->surface, &pRenderContext->surfaceCapabilities);
		FreeSwapchain(pRenderContext);
		CreateSwapchain(pRenderContext);
	}
}