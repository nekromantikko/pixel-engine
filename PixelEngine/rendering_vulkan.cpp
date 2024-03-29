#ifdef PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR
	#include <imgui_impl_win32.h>
#endif

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <cstring>
#include "rendering.h"
#include "system.h"
#include <imgui_impl_vulkan.h>

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
		Buffer timeBuffer;
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

		Buffer scanlineBuffer;

		// 8 palettes of 8 colors
		Buffer palTableBuffer;

		// Pattern table
		Buffer chrBuffer;

		// Nametable
		Buffer nametableBuffer;

		// OAM
		Buffer oamBuffer;

		Image colorImage;

		VkDescriptorSetLayout computeDescriptorSetLayout;
		VkDescriptorSet computeDescriptorSet;
		VkPipelineLayout softwarePipelineLayout;
		VkPipeline softwarePipeline;
		VkShaderModule softwareShaderModule;
		VkPipelineLayout evaluatePipelineLayout;
		VkPipeline evaluatePipeline;
		VkShaderModule evaluateShaderModule;

		// Render state from game code...
		Buffer renderStateBuffer;

		// Settings
		Settings settings;

		// Debug stuff
		bool32 renderDebugChr = false;
		Image debugChrImage[8];
		VkPipelineLayout chrPipelineLayout;
		VkPipeline chrPipeline;
		VkShaderModule chrShaderModule;
		VkDescriptorSet chrDescriptorSet[8];

		bool32 renderDebugPalette = false;
		Image debugPaletteImage;
		VkPipelineLayout palettePipelineLayout;
		VkPipeline palettePipeline;
		VkShaderModule paletteShaderModule;
		VkDescriptorSet paletteDescriptorSet;
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
			DEBUG_ERROR("Failed to create instance!\n");
		}
	}

	bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, u32 &outQueueFamilyIndex) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

		u32 extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
		VkExtensionProperties* availableExtensions = (VkExtensionProperties *)calloc(extensionCount, sizeof(VkExtensionProperties));
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
		VkSurfaceFormatKHR* availableFormats = (VkSurfaceFormatKHR *)calloc(surfaceFormatCount, sizeof(VkSurfaceFormatKHR));
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

	void GetSuitablePhysicalDevice(RenderContext* pContext) {
		u32 physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(pContext->instance, &physicalDeviceCount, nullptr);
		if (physicalDeviceCount == 0) {
			DEBUG_ERROR("No devices found for some reason!\n");
		}
		VkPhysicalDevice *availableDevices = (VkPhysicalDevice *)calloc(physicalDeviceCount, sizeof(VkPhysicalDevice));
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
			DEBUG_ERROR("No suitable physical device found!\n");
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
			DEBUG_ERROR("Failed to create logical device!\n");
		}
	}

	void CreateSwapchain(RenderContext* pContext) {
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
			DEBUG_ERROR("Failed to create shader module!\n");
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
			DEBUG_ERROR("failed to create render pass!");
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
		char* vertShader = AllocFileBytes("quad_vert.spv", vertShaderLength);
		u32 rawFragShaderLength;
		char* rawFragShader = AllocFileBytes("textured_raw_frag.spv", rawFragShaderLength);
		u32 CRTFragShaderLength;
		char* CRTFragShader = AllocFileBytes("textured_crt_frag.spv", CRTFragShaderLength);

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

	void FreeGraphicsPipeline(RenderContext* pRenderContext)
	{
		vkDestroyPipeline(pRenderContext->device, pRenderContext->blitRawPipeline, nullptr);
		vkDestroyPipeline(pRenderContext->device, pRenderContext->blitCRTPipeline, nullptr);
		vkDestroyPipelineLayout(pRenderContext->device, pRenderContext->blitPipelineLayout, nullptr);
		vkDestroyRenderPass(pRenderContext->device, pRenderContext->renderImagePass, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->blitVertexShaderModule, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->blitRawFragmentShaderModule, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->blitCRTFragmentShaderModule, nullptr);
		vkDestroyDescriptorSetLayout(pRenderContext->device, pRenderContext->graphicsDescriptorSetLayout, nullptr);
	}

	void AllocateMemory(RenderContext* pContext, VkMemoryRequirements requirements, VkMemoryPropertyFlags properties, VkDeviceMemory& outMemory) {
		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllocInfo.allocationSize = requirements.size;
		memAllocInfo.memoryTypeIndex = GetDeviceMemoryTypeIndex(pContext, requirements.memoryTypeBits, properties);

		VkResult err = vkAllocateMemory(pContext->device, &memAllocInfo, nullptr, &outMemory);
		if (err != VK_SUCCESS) {
			DEBUG_ERROR("Failed to allocate memory!\n");
		}
	}

	void AllocateBuffer(RenderContext* pContext, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, Buffer& outBuffer) {
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

		AllocateMemory(pContext, memRequirements, memProps, outBuffer.memory);

		vkBindBufferMemory(pContext->device, outBuffer.buffer, outBuffer.memory, 0);
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

	void FreeBuffer(RenderContext* pContext, Buffer buffer) {
		vkDestroyBuffer(pContext->device, buffer.buffer, nullptr);
		vkFreeMemory(pContext->device, buffer.memory, nullptr);
	}

	void CreateImage(RenderContext* pContext, u32 width, u32 height, VkImageType type, VkImageUsageFlags usage, Image& outImage) {
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

		AllocateMemory(pContext, memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImage.memory);
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

	void FreeImage(RenderContext* pContext, Image image) {
		vkDestroyImageView(pContext->device, image.view, nullptr);
		vkDestroyImage(pContext->device, image.image, nullptr);
		vkFreeMemory(pContext->device, image.memory, nullptr);
	}

	VkImageMemoryBarrier GetImageBarrier(Image* pImage, VkImageLayout oldLayout, VkImageLayout newLayout) {
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

	void CreateOAM(RenderContext* pContext) {
		u32 oamSize = MAX_SPRITE_COUNT * sizeof(Sprite);
		AllocateBuffer(pContext, oamSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pContext->oamBuffer);
	}

	void CreateNametable(RenderContext* pContext) {
		DEBUG_LOG("Creating nametables...\n");
		AllocateBuffer(pContext, NAMETABLE_SIZE * NAMETABLE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pContext->nametableBuffer);
	}

	void CreatePaletteTable(RenderContext* pContext) {
		u32 palFileSize;
		char* palData = AllocFileBytes("palette.dat", palFileSize);

		if (palFileSize < 8 * 8) {
			DEBUG_ERROR("Invalid palette table file!\n");
		}

		AllocateBuffer(pContext, 8 * 8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pContext->palTableBuffer);

		void* data;
		vkMapMemory(pContext->device, pContext->palTableBuffer.memory, 0, 8 * 8, 0, &data);
		memcpy(data, palData, 8 * 8);
		vkUnmapMemory(pContext->device, pContext->palTableBuffer.memory);
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
			DEBUG_ERROR("Invalid palette file!\n");
		}
		RIFFHeader* header = (RIFFHeader*)palBin;
		DEBUG_LOG("%.4s\n", header->signature);
		DEBUG_LOG("%.4s\n", header->type);
		if (strncmp(header->signature, "RIFF", 4) != 0 || strncmp(header->type, "PAL ", 4) != 0 || header->size + 8 != palFileSize){
			DEBUG_ERROR("Invalid palette file!\n");
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
			DEBUG_ERROR("No palette data found in file!\n");
		}

		Buffer stagingBuffer;
		AllocateBuffer(pContext, paletteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);

		void* data;
		vkMapMemory(pContext->device, stagingBuffer.memory, 0, paletteSize, 0, &data);
		memcpy(data, paletteData, paletteSize);
		vkUnmapMemory(pContext->device, stagingBuffer.memory);
		free(palBin);

		CreateImage(pContext, 64, 1, VK_IMAGE_TYPE_1D, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, pContext->paletteImage);

		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

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
			64,
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

		FreeBuffer(pContext, stagingBuffer);
	}

	RenderContext *CreateRenderContext(Surface surface) {
		RenderContext *context = (RenderContext*)calloc(1, sizeof(RenderContext));
		if (context == nullptr) {
			DEBUG_ERROR("Couldn't allocate memory for renderContext\n");
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
			DEBUG_ERROR("failed to create command pool!");
		}

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = context->primaryCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = COMMAND_BUFFER_COUNT;

		if (vkAllocateCommandBuffers(context->device, &allocInfo, context->primaryCommandBuffers) != VK_SUCCESS) {
			DEBUG_ERROR("failed to allocate command buffers!");
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

		vkCreateSampler(context->device, &samplerInfo, nullptr, &context->defaultSampler);

		// Compute resources
		CreatePalette(context);
		CreatePaletteTable(context);
		AllocateBuffer(context, CHR_MEMORY_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, context->chrBuffer);
		CreateNametable(context);
		CreateOAM(context);
		ClearSprites(context, 0, MAX_SPRITE_COUNT);
		AllocateBuffer(context, sizeof(ScanlineData) * SCANLINE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context->scanlineBuffer);
		AllocateBuffer(context, sizeof(RenderState) * SCANLINE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, context->renderStateBuffer);
		AllocateBuffer(context, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, context->timeBuffer);

		CreateImage(context, 512, 288, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, context->colorImage);

		// Write into descriptor sets...
		for (int i = 0; i < COMMAND_BUFFER_COUNT; i++) {
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = context->colorImage.view;
			imageInfo.sampler = context->defaultSampler;

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
		VkPipelineLayoutCreateInfo softwarePipelineLayoutInfo{};
		softwarePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		softwarePipelineLayoutInfo.setLayoutCount = 1;
		softwarePipelineLayoutInfo.pSetLayouts = &context->computeDescriptorSetLayout;
		softwarePipelineLayoutInfo.pPushConstantRanges = nullptr;
		softwarePipelineLayoutInfo.pushConstantRangeCount = 0;

		vkCreatePipelineLayout(context->device, &softwarePipelineLayoutInfo, nullptr, &context->softwarePipelineLayout);

		u32 softwareShaderLength;
		char* softwareShader = AllocFileBytes("software_comp.spv", softwareShaderLength);
		context->softwareShaderModule = CreateShaderModule(context->device, softwareShader, softwareShaderLength);
		free(softwareShader);

		VkPipelineShaderStageCreateInfo compShaderStageInfo{};
		compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		compShaderStageInfo.module = context->softwareShaderModule;
		compShaderStageInfo.pName = "main";

		VkComputePipelineCreateInfo computeCreateInfo{};
		computeCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computeCreateInfo.flags = 0;
		computeCreateInfo.stage = compShaderStageInfo;
		computeCreateInfo.layout = context->softwarePipelineLayout;

		vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1, &computeCreateInfo, nullptr, &context->softwarePipeline);

		VkDescriptorImageInfo perkele{};
		perkele.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		perkele.imageView = context->colorImage.view;
		perkele.sampler = context->defaultSampler;

		VkDescriptorImageInfo paletteBufferInfo{};
		paletteBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		paletteBufferInfo.imageView = context->paletteImage.view;
		paletteBufferInfo.sampler = context->defaultSampler;

		VkDescriptorBufferInfo chrBufferInfo{};
		chrBufferInfo.buffer = context->chrBuffer.buffer;
		chrBufferInfo.offset = 0;
		chrBufferInfo.range = CHR_MEMORY_SIZE;

		VkDescriptorBufferInfo palTableInfo{};
		palTableInfo.buffer = context->palTableBuffer.buffer;
		palTableInfo.offset = 0;
		palTableInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo nametableInfo{};
		nametableInfo.buffer = context->nametableBuffer.buffer;
		nametableInfo.offset = 0;
		nametableInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo oamInfo{};
		oamInfo.buffer = context->oamBuffer.buffer;
		oamInfo.offset = 0;
		oamInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo scanlineInfo{};
		scanlineInfo.buffer = context->scanlineBuffer.buffer;
		scanlineInfo.offset = 0;
		scanlineInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo renderStateInfo{};
		renderStateInfo.buffer = context->renderStateBuffer.buffer;
		renderStateInfo.offset = 0;
		renderStateInfo.range = VK_WHOLE_SIZE;

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

		context->settings = defaultSettings;
		return context;
	}

	void WaitForAllCommands(RenderContext* pRenderContext) {
		vkWaitForFences(pRenderContext->device, COMMAND_BUFFER_COUNT, pRenderContext->commandBufferFences, VK_TRUE, UINT64_MAX);
	}

	void FreeRenderContext(RenderContext* pRenderContext) {
		if (pRenderContext == nullptr) {
			return;
		}

		// Wait for all commands to execute first
		WaitForAllCommands(pRenderContext);

		// Free imgui stuff
		vkDestroyDescriptorPool(pRenderContext->device, pRenderContext->imGuiDescriptorPool, nullptr);

		// vkFreeDescriptorSets(pRenderContext->device, pRenderContext->descriptorPool, COMMAND_BUFFER_COUNT, pRenderContext->graphicsDescriptorSets);
		// vkFreeDescriptorSets(pRenderContext->device, pRenderContext->descriptorPool, 1, &pRenderContext->computeDescriptorSet);
		vkDestroyDescriptorPool(pRenderContext->device, pRenderContext->descriptorPool, nullptr);

		for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
			vkDestroySemaphore(pRenderContext->device, pRenderContext->imageAcquiredSemaphores[i], nullptr);
			vkDestroySemaphore(pRenderContext->device, pRenderContext->drawCompleteSemaphores[i], nullptr);
			vkDestroyFence(pRenderContext->device, pRenderContext->commandBufferFences[i], nullptr);
		}
		vkFreeCommandBuffers(pRenderContext->device, pRenderContext->primaryCommandPool, COMMAND_BUFFER_COUNT, pRenderContext->primaryCommandBuffers);
		vkDestroyCommandPool(pRenderContext->device, pRenderContext->primaryCommandPool, nullptr);

		FreeSwapchain(pRenderContext);

		FreeGraphicsPipeline(pRenderContext);

		vkDestroySampler(pRenderContext->device, pRenderContext->defaultSampler, nullptr);
		vkDestroyDescriptorSetLayout(pRenderContext->device, pRenderContext->computeDescriptorSetLayout, nullptr);

		vkDestroyPipeline(pRenderContext->device, pRenderContext->softwarePipeline, nullptr);
		vkDestroyPipelineLayout(pRenderContext->device, pRenderContext->softwarePipelineLayout, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->softwareShaderModule, nullptr);

		vkDestroyPipeline(pRenderContext->device, pRenderContext->evaluatePipeline, nullptr);
		vkDestroyPipelineLayout(pRenderContext->device, pRenderContext->evaluatePipelineLayout, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->evaluateShaderModule, nullptr);
		FreeImage(pRenderContext, pRenderContext->colorImage);
		FreeImage(pRenderContext, pRenderContext->paletteImage);

		vkDestroyPipeline(pRenderContext->device, pRenderContext->chrPipeline, nullptr);
		vkDestroyPipelineLayout(pRenderContext->device, pRenderContext->chrPipelineLayout, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->chrShaderModule, nullptr);
		for (int i = 0; i < 8; i++) {
			FreeImage(pRenderContext, pRenderContext->debugChrImage[i]);
		}

		vkDestroyPipeline(pRenderContext->device, pRenderContext->palettePipeline, nullptr);
		vkDestroyPipelineLayout(pRenderContext->device, pRenderContext->palettePipelineLayout, nullptr);
		vkDestroyShaderModule(pRenderContext->device, pRenderContext->paletteShaderModule, nullptr);
		FreeImage(pRenderContext, pRenderContext->debugPaletteImage);

		FreeBuffer(pRenderContext, pRenderContext->chrBuffer);
		FreeBuffer(pRenderContext, pRenderContext->nametableBuffer);
		FreeBuffer(pRenderContext, pRenderContext->oamBuffer);
		FreeBuffer(pRenderContext, pRenderContext->palTableBuffer);
		FreeBuffer(pRenderContext, pRenderContext->renderStateBuffer);
		FreeBuffer(pRenderContext, pRenderContext->scanlineBuffer);
		FreeBuffer(pRenderContext, pRenderContext->timeBuffer);

		vkDestroyDevice(pRenderContext->device, nullptr);
		vkDestroySurfaceKHR(pRenderContext->instance, pRenderContext->surface, nullptr);
		vkDestroyInstance(pRenderContext->instance, nullptr);
		free(pRenderContext);
	}

	//////////////////////////////////////////////////////

	void ReadPaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* outColors) {
		if (offset + count > 8 || paletteIndex >= 8) {
			DEBUG_ERROR("Trying to get palette colors outside range!\n");
		}

		u32 actualOffset = 8 * paletteIndex + offset;

		void* data;
		vkMapMemory(pContext->device, pContext->palTableBuffer.memory, actualOffset, count, 0, &data);
		memcpy(outColors, data, count);
		vkUnmapMemory(pContext->device, pContext->palTableBuffer.memory);
	}
	void WritePaletteColors(RenderContext* pContext, u8 paletteIndex, u32 count, u32 offset, u8* colors) {
		if (offset + count > 8 || paletteIndex >= 8) {
			DEBUG_ERROR("Trying to set palette colors outside range!\n");
		}

		u32 actualOffset = 8 * paletteIndex + offset;

		void* data;
		vkMapMemory(pContext->device, pContext->palTableBuffer.memory, actualOffset, count, 0, &data);
		memcpy(data, colors, count);
		vkUnmapMemory(pContext->device, pContext->palTableBuffer.memory);
	}
	// This is super slow, optimize pls
	void ClearSprites(RenderContext* pContext, u32 offset, u32 count) {
		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(temp, &beginInfo);

		vkCmdFillBuffer(
			temp,
			pContext->oamBuffer.buffer,
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
	void ReadSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* outSprites) {
		if (offset + count > MAX_SPRITE_COUNT) {
			DEBUG_ERROR("Trying to read sprites outside range!\n");
		}

		u32 actualOffset = offset * sizeof(Sprite);
		u32 size = count * sizeof(Sprite);

		void* data;
		vkMapMemory(pContext->device, pContext->oamBuffer.memory, actualOffset, size, 0, &data);
		memcpy((void*)outSprites, data, size);
		vkUnmapMemory(pContext->device, pContext->oamBuffer.memory);
	}
	void WriteSprites(RenderContext* pContext, u32 count, u32 offset, Sprite* sprites) {
		if (count == 0) {
			return;
		}

		if (offset + count > MAX_SPRITE_COUNT) {
			DEBUG_ERROR("Trying to write sprites outside range!\n");
		}

		u32 actualOffset = offset * sizeof(Sprite);
		u32 size = count * sizeof(Sprite);

		void* data;
		vkMapMemory(pContext->device, pContext->oamBuffer.memory, actualOffset, size, 0, &data);
		memcpy(data, (void*)sprites, size);
		vkUnmapMemory(pContext->device, pContext->oamBuffer.memory);
	}
	void WriteChrMemory(RenderContext* pContext, u32 size, u32 offset, u8* bytes) {
		if (offset + size > CHR_MEMORY_SIZE) {
			DEBUG_ERROR("Trying to write chr memory outside range!\n");
		}

		void* data;
		vkMapMemory(pContext->device, pContext->chrBuffer.memory, offset, size, 0, &data);
		memcpy(data, (void*)bytes, size);
		vkUnmapMemory(pContext->device, pContext->chrBuffer.memory);
	}
	void ReadChrMemory(RenderContext* pContext, u32 size, u32 offset, u8* outBytes) {
		if (offset + size > CHR_MEMORY_SIZE) {
			DEBUG_ERROR("Trying to read chr memory outside range!\n");
		}

		void* data;
		vkMapMemory(pContext->device, pContext->chrBuffer.memory, offset, size, 0, &data);
		memcpy((void*)outBytes, data, size);
		vkUnmapMemory(pContext->device, pContext->chrBuffer.memory);
	}
	void WriteNametable(RenderContext* pContext, u16 index, u16 count, u16 offset, u8* tiles) {
		void* data;
		vkMapMemory(pContext->device, pContext->nametableBuffer.memory, index * NAMETABLE_SIZE + offset, count, 0, &data);
		memcpy(data, tiles, count);
		vkUnmapMemory(pContext->device, pContext->nametableBuffer.memory);
	}
	void ReadNametable(RenderContext* pContext, u16 index, u16 count, u16 offset, u8* outTiles) {
		void* data;
		vkMapMemory(pContext->device, pContext->nametableBuffer.memory, index * NAMETABLE_SIZE + offset, count, 0, &data);
		memcpy((void*)outTiles, data, count);
		vkUnmapMemory(pContext->device, pContext->nametableBuffer.memory);
	}

	void SetRenderState(RenderContext* pContext, u32 scanlineOffset, u32 scanlineCount, RenderState state) {
		void* data;
		vkMapMemory(pContext->device, pContext->renderStateBuffer.memory, sizeof(RenderState) * scanlineOffset, sizeof(RenderState) * scanlineCount, 0, &data);
		RenderState* scanlineStates = (RenderState*)data;
		for (int i = 0; i < scanlineCount; i++) {
			memcpy(scanlineStates + i, &state, sizeof(RenderState));
		}
		vkUnmapMemory(pContext->device, pContext->renderStateBuffer.memory);
	}

	//////////////////////////////////////////////////////

	void InitImGui(RenderContext* pContext) {
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

		ImGui_ImplVulkan_Init(&vulkanInitInfo, pContext->renderImagePass);

		// Generate fonts
		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(temp, &beginInfo);

		ImGui_ImplVulkan_CreateFontsTexture(temp);

		vkEndCommandBuffer(temp);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &temp;

		vkQueueSubmit(pContext->primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(pContext->primaryQueue);

		vkFreeCommandBuffers(pContext->device, pContext->primaryCommandPool, 1, &temp);
	}
	void BeginImGuiFrame(RenderContext* pContext) {
		ImGui_ImplVulkan_NewFrame();
#ifdef PLATFORM_WINDOWS
		ImGui_ImplWin32_NewFrame();
#endif
	}
	void ShutdownImGui() {
		ImGui_ImplVulkan_Shutdown();
#ifdef PLATFORM_WINDOWS
		ImGui_ImplWin32_Shutdown();
#endif
	}

	//////////////////////////////////////////////////////

	/// DEBUG STUFF ///
	Settings* GetSettingsPtr(RenderContext* pContext) {
		return &pContext->settings;
	}

	void DebugRenderChr(RenderContext* pContext, VkCommandBuffer cmd) {
		for (int i = 0; i < 8; i++) {
			// Convert from preinitialized to general
			VkImageMemoryBarrier barrier = GetImageBarrier(&pContext->debugChrImage[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			vkCmdPipelineBarrier(
				cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->chrPipelineLayout, 0, 1, &pContext->chrDescriptorSet[i], 0, nullptr);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->chrPipeline);
			vkCmdPushConstants(cmd, pContext->chrPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &i);
			vkCmdDispatch(cmd, 32, 16, 1);

			// Convert from preinitialized to general
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
	}

	ImTextureID* SetupDebugChrRendering(RenderContext* pContext) {
		ImTextureID* textures = (ImTextureID*)calloc(8, sizeof(ImTextureID));

		VkDescriptorSetLayout layouts[8] = {
			pContext->computeDescriptorSetLayout,
			pContext->computeDescriptorSetLayout,
			pContext->computeDescriptorSetLayout,
			pContext->computeDescriptorSetLayout,
			pContext->computeDescriptorSetLayout,
			pContext->computeDescriptorSetLayout,
			pContext->computeDescriptorSetLayout,
			pContext->computeDescriptorSetLayout
		};
		VkDescriptorSetAllocateInfo chrDescriptorSetAllocInfo{};
		chrDescriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		chrDescriptorSetAllocInfo.descriptorPool = pContext->descriptorPool;
		chrDescriptorSetAllocInfo.descriptorSetCount = 8;
		chrDescriptorSetAllocInfo.pSetLayouts = layouts;

		VkResult res = vkAllocateDescriptorSets(pContext->device, &chrDescriptorSetAllocInfo, pContext->chrDescriptorSet);
		if (res != VK_SUCCESS) {
			DEBUG_ERROR("Whoopsie poopsie :c %d\n", res);
		}

		for (int i = 0; i < 8; i++) {
			CreateImage(pContext, 256, 128, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, pContext->debugChrImage[i]);

			textures[i] = (ImTextureID)ImGui_ImplVulkan_AddTexture(pContext->defaultSampler, pContext->debugChrImage[i].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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
			chrBufferInfo.buffer = pContext->chrBuffer.buffer;
			chrBufferInfo.offset = 0;
			chrBufferInfo.range = CHR_MEMORY_SIZE;

			VkDescriptorBufferInfo palTableInfo{};
			palTableInfo.buffer = pContext->palTableBuffer.buffer;
			palTableInfo.offset = 0;
			palTableInfo.range = VK_WHOLE_SIZE;

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
		char* chrShader = AllocFileBytes("debug_blit_chr_comp.spv", chrShaderLength);
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

		/////////////////////////////////////////////////////

		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(temp, &beginInfo);

		DebugRenderChr(pContext, temp);

		vkEndCommandBuffer(temp);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &temp;

		vkQueueSubmit(pContext->primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(pContext->primaryQueue);

		vkFreeCommandBuffers(pContext->device, pContext->primaryCommandPool, 1, &temp);

		pContext->renderDebugChr = true;
		return textures;
	}

	void DebugRenderPalette(RenderContext* pContext, VkCommandBuffer cmd) {
		// Convert from preinitialized to general
		VkImageMemoryBarrier barrier = GetImageBarrier(&pContext->debugPaletteImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		vkCmdPipelineBarrier(
			cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->palettePipelineLayout, 0, 1, &pContext->paletteDescriptorSet, 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pContext->palettePipeline);
		vkCmdDispatch(cmd, 32, 16, 1);

		// Convert from preinitialized to general
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

	ImTextureID SetupDebugPaletteRendering(RenderContext* pContext) {
		CreateImage(pContext, 64, 1, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, pContext->debugPaletteImage);

		VkPipelineLayoutCreateInfo chrPipelineLayoutInfo{};
		chrPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		chrPipelineLayoutInfo.setLayoutCount = 1;
		chrPipelineLayoutInfo.pSetLayouts = &pContext->computeDescriptorSetLayout;

		vkCreatePipelineLayout(pContext->device, &chrPipelineLayoutInfo, nullptr, &pContext->palettePipelineLayout);

		u32 chrShaderLength;
		char* chrShader = AllocFileBytes("debug_blit_pal_comp.spv", chrShaderLength);
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
		chrBufferInfo.buffer = pContext->chrBuffer.buffer;
		chrBufferInfo.offset = 0;
		chrBufferInfo.range = CHR_MEMORY_SIZE;

		VkDescriptorBufferInfo palTableInfo{};
		palTableInfo.buffer = pContext->palTableBuffer.buffer;
		palTableInfo.offset = 0;
		palTableInfo.range = VK_WHOLE_SIZE;

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

		/////////////////////////////////////////////////////

		VkCommandBuffer temp = GetTemporaryCommandBuffer(pContext);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(temp, &beginInfo);

		DebugRenderPalette(pContext, temp);

		vkEndCommandBuffer(temp);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &temp;

		vkQueueSubmit(pContext->primaryQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(pContext->primaryQueue);

		vkFreeCommandBuffers(pContext->device, pContext->primaryCommandPool, 1, &temp);

		pContext->renderDebugPalette = true;
		return (ImTextureID)ImGui_ImplVulkan_AddTexture(pContext->defaultSampler, pContext->debugPaletteImage.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	//////////////////////////////////////////////////////

	void SetCurrentTime(RenderContext* pContext, float seconds) {
		void* data;
		vkMapMemory(pContext->device, pContext->timeBuffer.memory, 0, sizeof(float), 0, &data);
		memcpy(data, &seconds, sizeof(float));
		vkUnmapMemory(pContext->device, pContext->timeBuffer.memory);
	}

	void RunSoftwareRenderer(RenderContext* pContext) {
		VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

		// Convert from preinitialized to general
		VkImageMemoryBarrier imageBarrier = GetImageBarrier(&pContext->colorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier
		);

		// Clear old scanline data
		vkCmdFillBuffer(
			commandBuffer,
			pContext->scanlineBuffer.buffer,
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
		vkCmdDispatch(commandBuffer, 16, 9, 1);

		// Convert from preinitialized to general
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier
		);

		if (pContext->renderDebugChr) {
			DebugRenderChr(pContext, commandBuffer);
		}
		if (pContext->renderDebugPalette) {
			DebugRenderPalette(pContext, commandBuffer);
		}
	}

	void BeginRenderPass(RenderContext* pContext) {
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

	void BlitSoftwareResults(RenderContext* pContext, Quad quad) {
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

	void EndRenderPass(RenderContext* pContext) {
		VkCommandBuffer commandBuffer = pContext->primaryCommandBuffers[pContext->currentCbIndex];

		// Render ImGui
		ImDrawData* drawData = ImGui::GetDrawData();
		if (drawData != nullptr) {
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
		}

		vkCmdEndRenderPass(commandBuffer);
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
			DEBUG_ERROR("failed to begin recording command buffer!");
		}

		// Should be ready to draw now!
	}
	
	void EndDraw(RenderContext* pRenderContext) {
		VkCommandBuffer commandBuffer = pRenderContext->primaryCommandBuffers[pRenderContext->currentCbIndex];

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			DEBUG_ERROR("failed to record command buffer!");
		}

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

	void Render(RenderContext* pContext) {
		// Just run all the commands
		BeginDraw(pContext);
		RunSoftwareRenderer(pContext);
		BeginRenderPass(pContext);
		BlitSoftwareResults(pContext, defaultQuad);
		EndRenderPass(pContext);
		EndDraw(pContext);
	}

	void ResizeSurface(RenderContext* pRenderContext, u32 width, u32 height) {
		// Wait for all commands to execute first
		vkWaitForFences(pRenderContext->device, COMMAND_BUFFER_COUNT, pRenderContext->commandBufferFences, VK_TRUE, UINT64_MAX);

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderContext->physicalDevice, pRenderContext->surface, &pRenderContext->surfaceCapabilities);
		FreeSwapchain(pRenderContext);
		CreateSwapchain(pRenderContext);
	}
}