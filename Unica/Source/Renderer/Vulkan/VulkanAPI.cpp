// 2021-2022 Copyright joaofonseca.dev, All Rights Reserved.

#include "VulkanAPI.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "UnicaFileUtilities.h"
#include "GLFW/glfw3.h"
#include "fmt/format.h"
#include "shaderc/shaderc.hpp"

#include "UnicaMinimal.h"
#include "VulkanQueueFamilyIndices.h"
#include "Renderer/RenderManager.h"
#include "Shaders/ShaderUtilities.h"

void VulkanAPI::Init()
{
	m_GlfwRenderWindow = std::make_unique_for_overwrite<GlfwRenderWindow>();
	
	CreateVulkanInstance();
	CreateVulkanDebugMessenger();
	CreateVulkanWindowSurface();
	SelectVulkanPhysicalDevice();
	CreateVulkanLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
}

void VulkanAPI::Tick()
{
	m_GlfwRenderWindow->Tick();
}

void VulkanAPI::CreateVulkanInstance()
{
	VkApplicationInfo VulkanAppInfo{ };
	VulkanAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	VulkanAppInfo.pApplicationName = UnicaSettings::ApplicationName.c_str();
	VulkanAppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	VulkanAppInfo.pEngineName = UnicaSettings::EngineName.c_str();
	VulkanAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	VulkanAppInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo VulkanCreateInfo{ };
	VkDebugUtilsMessengerCreateInfoEXT VulkanDebugCreateInfo;
	VulkanCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	VulkanCreateInfo.pApplicationInfo = &VulkanAppInfo;
	VulkanCreateInfo.enabledLayerCount = 0;
	VulkanCreateInfo.pNext = nullptr;
    
#ifdef __APPLE__
    VulkanCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif // __APPLE__

	uint32 GlfwExtensionCount = 0;
	const char** GlfwExtensions = glfwGetRequiredInstanceExtensions(&GlfwExtensionCount);
	std::vector<const char*> RequiredExtensions(GlfwExtensions, GlfwExtensions + GlfwExtensionCount);

	if (m_bValidationLayersEnabled)
	{
		RequiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	AddRequiredExtensions(VulkanCreateInfo, RequiredExtensions);
	AddValidationLayers(VulkanCreateInfo, VulkanDebugCreateInfo);

	if (vkCreateInstance(&VulkanCreateInfo, nullptr, &m_VulkanInstance) != VK_SUCCESS)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Couldn't create Vulkan instance");
		return;
	}
	UNICA_LOG(Log, __FUNCTION__, "Vulkan instance created");
}

void VulkanAPI::SelectVulkanPhysicalDevice()
{
	uint32 VulkanPhysicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(m_VulkanInstance, &VulkanPhysicalDeviceCount, nullptr);
	if (VulkanPhysicalDeviceCount < 1)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "No GPUs with Vulkan support found");
		return;
	}

	std::vector<VkPhysicalDevice> VulkanPhysicalDevices(VulkanPhysicalDeviceCount);
	vkEnumeratePhysicalDevices(m_VulkanInstance, &VulkanPhysicalDeviceCount, VulkanPhysicalDevices.data());

	std::multimap<uint32, VkPhysicalDevice> VulkanPhysicalDeviceCandidates;
	for (const VkPhysicalDevice& VulkanPhysicalDevice : VulkanPhysicalDevices)
	{
		uint32 VulkanPhysicalDeviceScore = RateVulkanPhysicalDevice(VulkanPhysicalDevice);
		VulkanPhysicalDeviceCandidates.insert(std::make_pair(VulkanPhysicalDeviceScore, VulkanPhysicalDevice));
	}

	if (VulkanPhysicalDeviceCandidates.rbegin()->first > 0)
	{
		m_VulkanPhysicalDevice = VulkanPhysicalDeviceCandidates.rbegin()->second;
	}
	else
	{
		UNICA_LOG(Fatal, __FUNCTION__, "No suitable GPU found");
		return;
	}
}

uint32 VulkanAPI::RateVulkanPhysicalDevice(const VkPhysicalDevice& VulkanPhysicalDevice)
{
	if (!GetDeviceQueueFamilies(VulkanPhysicalDevice).WasSet())
	{
		return 0;
	}

	if (!DeviceHasRequiredExtensions(VulkanPhysicalDevice))
	{
		return 0;
	}

	const VulkanSwapChainSupportDetails SwapChainSupportDetails = QuerySwapChainSupport(VulkanPhysicalDevice);
	if (SwapChainSupportDetails.SurfaceFormats.empty() || SwapChainSupportDetails.PresentModes.empty())
	{
		return 0;
	}
	
	uint32 Score = 1;

	VkPhysicalDeviceProperties VulkanPhysicalDeviceProperties;
	VkPhysicalDeviceFeatures VulkanPhysicalDeviceFeatures;
	vkGetPhysicalDeviceProperties(VulkanPhysicalDevice, &VulkanPhysicalDeviceProperties);
	vkGetPhysicalDeviceFeatures(VulkanPhysicalDevice, &VulkanPhysicalDeviceFeatures);

	if (VulkanPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		Score += 1000;
	}
	
	return Score;
}

bool VulkanAPI::DeviceHasRequiredExtensions(const VkPhysicalDevice& VulkanPhysicalDevice)
{
	uint32_t AvailableDeviceExtensionCount;
	vkEnumerateDeviceExtensionProperties(VulkanPhysicalDevice, nullptr, &AvailableDeviceExtensionCount, nullptr);
	std::vector<VkExtensionProperties> AvailableDeviceExtensions(AvailableDeviceExtensionCount);
	vkEnumerateDeviceExtensionProperties(VulkanPhysicalDevice, nullptr, &AvailableDeviceExtensionCount, AvailableDeviceExtensions.data());

	std::set<std::string> DeviceExtensions(m_RequiredDeviceExtensions.begin(), m_RequiredDeviceExtensions.end());
	for (const VkExtensionProperties& DeviceExtension : AvailableDeviceExtensions)
	{
		DeviceExtensions.erase(DeviceExtension.extensionName);
	}

	return DeviceExtensions.empty();
}

VulkanQueueFamilyIndices VulkanAPI::GetDeviceQueueFamilies(const VkPhysicalDevice& VulkanPhysicalDevice)
{
	VulkanQueueFamilyIndices QueueFamilyIndices;

	uint32 QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(VulkanPhysicalDevice, &QueueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(VulkanPhysicalDevice, &QueueFamilyCount, QueueFamilies.data());

	uint32 QueueFamilyIndex = 0;
	for (const VkQueueFamilyProperties& QueueFamily : QueueFamilies)
	{
		if (QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			QueueFamilyIndices.SetGraphicsFamily(QueueFamilyIndex);
		}

		VkBool32 PresentImagesSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(VulkanPhysicalDevice, QueueFamilyIndex, m_VulkanWindowSurface, &PresentImagesSupport);

		if (PresentImagesSupport)
		{
			QueueFamilyIndices.SetPresentImagesFamily(QueueFamilyIndex);
		}

		if (QueueFamilyIndices.WasSet())
		{
			break;
		}
		
		QueueFamilyIndex++;
	}
	
	return QueueFamilyIndices;
}

VulkanSwapChainSupportDetails VulkanAPI::QuerySwapChainSupport(const VkPhysicalDevice& VulkanPhysicalDevice)
{
	VulkanSwapChainSupportDetails SwapChainSupportDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanPhysicalDevice, m_VulkanWindowSurface, &SwapChainSupportDetails.SurfaceCapabilities);

	uint32 SurfaceFormatsCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(VulkanPhysicalDevice, m_VulkanWindowSurface, &SurfaceFormatsCount, nullptr);
	if (SurfaceFormatsCount > 0)
	{
		SwapChainSupportDetails.SurfaceFormats.resize(SurfaceFormatsCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(VulkanPhysicalDevice, m_VulkanWindowSurface, &SurfaceFormatsCount, SwapChainSupportDetails.SurfaceFormats.data());
	}

	uint32 PresentModesCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(VulkanPhysicalDevice, m_VulkanWindowSurface, &PresentModesCount, nullptr);
	if (PresentModesCount > 0)
	{
		SwapChainSupportDetails.PresentModes.resize(PresentModesCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(VulkanPhysicalDevice, m_VulkanWindowSurface, &PresentModesCount, SwapChainSupportDetails.PresentModes.data());
	}
	
	return SwapChainSupportDetails;
}

VkSurfaceFormatKHR VulkanAPI::SelectSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableSurfaceFormats)
{
	for (const VkSurfaceFormatKHR& SurfaceFormat : AvailableSurfaceFormats)
	{
		if (SurfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && SurfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return SurfaceFormat;
		}
	}

	// TODO: Rank surface formats to default to the best possible one
	return AvailableSurfaceFormats[0];
}

VkPresentModeKHR VulkanAPI::SelectSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailablePresentModes)
{
	for (const VkPresentModeKHR& PresentMode : AvailablePresentModes)
	{
		if (PresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return PresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanAPI::SelectSwapExtent(const VkSurfaceCapabilitiesKHR& SurfaceCapabilities)
{
	if (SurfaceCapabilities.currentExtent.width != std::numeric_limits<uint32>::max())
	{
		return SurfaceCapabilities.currentExtent;
	}

	int32 Width, Height;
	glfwGetFramebufferSize(m_GlfwRenderWindow->GetGlfwWindow(), &Width, &Height);

	VkExtent2D VulkanExtent = { static_cast<uint32>(Width), static_cast<uint32>(Height) };
	VulkanExtent.width = std::clamp(VulkanExtent.width, SurfaceCapabilities.minImageExtent.width, SurfaceCapabilities.maxImageExtent.width);
	VulkanExtent.height = std::clamp(VulkanExtent.height, SurfaceCapabilities.minImageExtent.height, SurfaceCapabilities.maxImageExtent.height);

	return VulkanExtent;
}

void VulkanAPI::CreateSwapChain()
{
	const VulkanSwapChainSupportDetails SwapChainSupportDetails = QuerySwapChainSupport(m_VulkanPhysicalDevice);
	const VkSurfaceFormatKHR SurfaceFormat = SelectSwapSurfaceFormat(SwapChainSupportDetails.SurfaceFormats);
	const VkPresentModeKHR PresentMode = SelectSwapPresentMode(SwapChainSupportDetails.PresentModes);
	const VkExtent2D Extent = SelectSwapExtent(SwapChainSupportDetails.SurfaceCapabilities);

	uint32 SwapImageCount = SwapChainSupportDetails.SurfaceCapabilities.minImageCount + 1;
	if (SwapChainSupportDetails.SurfaceCapabilities.maxImageCount > 0 && SwapImageCount > SwapChainSupportDetails.SurfaceCapabilities.maxImageCount)
	{
		SwapImageCount = SwapChainSupportDetails.SurfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR SwapChainCreateInfo { };
	SwapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	SwapChainCreateInfo.surface = m_VulkanWindowSurface;
	SwapChainCreateInfo.minImageCount = SwapImageCount;
	SwapChainCreateInfo.imageFormat = SurfaceFormat.format;
	SwapChainCreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
	SwapChainCreateInfo.imageExtent = Extent;
	SwapChainCreateInfo.imageArrayLayers = 1;
	SwapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VulkanQueueFamilyIndices QueueFamilyIndices = GetDeviceQueueFamilies(m_VulkanPhysicalDevice);
	const uint32 QueueFamilyIndicesArray[] = { QueueFamilyIndices.GetGraphicsFamily().value(), QueueFamilyIndices.GetPresentImagesFamily().value() };

	if (QueueFamilyIndices.GetGraphicsFamily() != QueueFamilyIndices.GetPresentImagesFamily())
	{
		SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		SwapChainCreateInfo.queueFamilyIndexCount = 2;
		SwapChainCreateInfo.pQueueFamilyIndices = QueueFamilyIndicesArray;
	}
	else
	{
		SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		SwapChainCreateInfo.queueFamilyIndexCount = 0;
		SwapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	SwapChainCreateInfo.preTransform = SwapChainSupportDetails.SurfaceCapabilities.currentTransform;
	SwapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	SwapChainCreateInfo.presentMode = PresentMode;
	SwapChainCreateInfo.clipped = VK_TRUE;
	SwapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_VulkanLogicalDevice, &SwapChainCreateInfo, nullptr, &m_VulkanSwapChain) != VK_SUCCESS)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Failed to create VulkanSwapChain");
		return;
	}

	vkGetSwapchainImagesKHR(m_VulkanLogicalDevice, m_VulkanSwapChain, &SwapImageCount, nullptr);
	m_VulkanSwapChainImages.resize(SwapImageCount);
	vkGetSwapchainImagesKHR(m_VulkanLogicalDevice, m_VulkanSwapChain, &SwapImageCount, m_VulkanSwapChainImages.data());

	m_VulkanSwapChainImageFormat = SurfaceFormat.format;
	m_VulkanSwapChainExtent = Extent;
}

void VulkanAPI::CreateImageViews()
{
	uint32 SwapChainImageIteration = 0;
	m_VulkanSwapChainImageViews.resize(m_VulkanSwapChainImages.size());
	for (const VkImage& SwapChainImage : m_VulkanSwapChainImages)
	{
		VkImageViewCreateInfo ImageViewCreateInfo { };
		ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ImageViewCreateInfo.image = SwapChainImage;
		ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ImageViewCreateInfo.format = m_VulkanSwapChainImageFormat;

		ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		ImageViewCreateInfo.subresourceRange.levelCount = 1;
		ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		ImageViewCreateInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_VulkanLogicalDevice, &ImageViewCreateInfo, nullptr, &m_VulkanSwapChainImageViews[SwapChainImageIteration]) != VK_SUCCESS)
		{
			UNICA_LOG(Fatal, __FUNCTION__, "Failed to create VulkanImageViews");
			return;
		}

		SwapChainImageIteration++;
	}
}

void VulkanAPI::CreateRenderPass()
{
	VkAttachmentDescription VulkanColorAttachment { };
	VulkanColorAttachment.format = m_VulkanSwapChainImageFormat;
	VulkanColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	VulkanColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	VulkanColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	VulkanColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	VulkanColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VulkanColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference VulkanColorAttachmentRef{};
	VulkanColorAttachmentRef.attachment = 0;
	VulkanColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription VulkanSubpass{};
	VulkanSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	VulkanSubpass.colorAttachmentCount = 1;
	VulkanSubpass.pColorAttachments = &VulkanColorAttachmentRef;

	VkRenderPassCreateInfo VulkanRenderPassCreateInfo{};
	VulkanRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	VulkanRenderPassCreateInfo.attachmentCount = 1;
	VulkanRenderPassCreateInfo.pAttachments = &VulkanColorAttachment;
	VulkanRenderPassCreateInfo.subpassCount = 1;
	VulkanRenderPassCreateInfo.pSubpasses = &VulkanSubpass;

	if (vkCreateRenderPass(m_VulkanLogicalDevice, &VulkanRenderPassCreateInfo, nullptr, &m_VulkanRenderPass) != VK_SUCCESS)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Failed to create VulkanRenderPass");
	}
}

void VulkanAPI::CreateGraphicsPipeline()
{
#if !UNICA_SHIPPING
	ShaderUtilities::CompileShaders();
#endif

	std::vector<char> VertShaderBinary = ShaderUtilities::LoadShader("Engine:Shaders/shader.vert");
	std::vector<char> FragShaderBinary = ShaderUtilities::LoadShader("Engine:Shaders/shader.frag");

	VkShaderModule VertShaderModule = CreateShaderModule(VertShaderBinary);
	VkShaderModule FragShaderModule = CreateShaderModule(FragShaderBinary);

	VkPipelineShaderStageCreateInfo VertPipelineShaderStageCreateInfo { };
	VertPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	VertPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	VertPipelineShaderStageCreateInfo.module = VertShaderModule;
	VertPipelineShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo FragPipelineShaderStageCreateInfo { };
	FragPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	FragPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	FragPipelineShaderStageCreateInfo.module = FragShaderModule;
	FragPipelineShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfos[] = { VertPipelineShaderStageCreateInfo, FragPipelineShaderStageCreateInfo };

	std::vector<VkDynamicState> PipelineDynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo PipelineDynamicCreateInfo { };
	PipelineDynamicCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	PipelineDynamicCreateInfo.dynamicStateCount = static_cast<uint32_t>(PipelineDynamicStates.size());
	PipelineDynamicCreateInfo.pDynamicStates = PipelineDynamicStates.data();

	VkPipelineVertexInputStateCreateInfo PipelineVertexInputCreateInfo { };
	PipelineVertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	PipelineVertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	PipelineVertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	PipelineVertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	PipelineVertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyCreateInfo{};
	PipelineInputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	PipelineInputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	PipelineInputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport Viewport{};
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.width = static_cast<float>(m_VulkanSwapChainExtent.width);
	Viewport.height = static_cast<float>(m_VulkanSwapChainExtent.height);
	Viewport.minDepth = 0.0f;
	Viewport.maxDepth = 1.0f;

	VkRect2D ScissorRectangle{};
	ScissorRectangle.offset = {0, 0};
	ScissorRectangle.extent = m_VulkanSwapChainExtent;

	VkPipelineViewportStateCreateInfo PipelineViewportCreateInfo { };
	PipelineViewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	PipelineViewportCreateInfo.viewportCount = 1;
	PipelineViewportCreateInfo.pViewports = &Viewport;
	PipelineViewportCreateInfo.scissorCount = 1;
	PipelineViewportCreateInfo.pScissors = &ScissorRectangle;

	VkPipelineRasterizationStateCreateInfo PipelineRasterizationCreateInfo { };
	PipelineRasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	PipelineRasterizationCreateInfo.depthClampEnable = VK_FALSE;
	PipelineRasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	PipelineRasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	PipelineRasterizationCreateInfo.lineWidth = 1.0f;
	PipelineRasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	PipelineRasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	PipelineRasterizationCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo PipelineMultisampleCreateInfo { };
	PipelineMultisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	PipelineMultisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	PipelineMultisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	PipelineMultisampleCreateInfo.minSampleShading = 1.0f;
	PipelineMultisampleCreateInfo.pSampleMask = nullptr;
	PipelineMultisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
	PipelineMultisampleCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState PipelineColorBlendAttachment { };
	PipelineColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	PipelineColorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo PipelineColorBlend { };
	PipelineColorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	PipelineColorBlend.logicOpEnable = VK_FALSE;
	PipelineColorBlend.attachmentCount = 1;
	PipelineColorBlend.pAttachments = &PipelineColorBlendAttachment;

	VkPipelineLayoutCreateInfo PipelineLayoutInfo { };
	PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(m_VulkanLogicalDevice, &PipelineLayoutInfo, nullptr, &m_VulkanPipelineLayout) != VK_SUCCESS)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Failed to create a VulkanPipelineLayout");
	}

	VkGraphicsPipelineCreateInfo GraphicsPipelineCreateInfo { };
	GraphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	GraphicsPipelineCreateInfo.stageCount = 2;
	GraphicsPipelineCreateInfo.pStages = PipelineShaderStageCreateInfos;
	GraphicsPipelineCreateInfo.pVertexInputState = &PipelineVertexInputCreateInfo;
	GraphicsPipelineCreateInfo.pInputAssemblyState = &PipelineInputAssemblyCreateInfo;
	GraphicsPipelineCreateInfo.pViewportState = &PipelineViewportCreateInfo;
	GraphicsPipelineCreateInfo.pRasterizationState = &PipelineRasterizationCreateInfo;
	GraphicsPipelineCreateInfo.pMultisampleState = &PipelineMultisampleCreateInfo;
	GraphicsPipelineCreateInfo.pColorBlendState = &PipelineColorBlend;
	GraphicsPipelineCreateInfo.pDynamicState = &PipelineDynamicCreateInfo;
	GraphicsPipelineCreateInfo.layout = m_VulkanPipelineLayout;
	GraphicsPipelineCreateInfo.renderPass = m_VulkanRenderPass;
	GraphicsPipelineCreateInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(m_VulkanLogicalDevice, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, nullptr, &m_VulkanGraphicsPipeline) != VK_SUCCESS)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Failed to create the VulkanGraphicsPipeline");
	}

	// Cleanup shader modules since they've already been created
	vkDestroyShaderModule(m_VulkanLogicalDevice, VertShaderModule, nullptr);
	vkDestroyShaderModule(m_VulkanLogicalDevice, FragShaderModule, nullptr);
}

VkShaderModule VulkanAPI::CreateShaderModule(const std::vector<char>& ShaderBinary)
{
	VkShaderModuleCreateInfo ShaderModuleCreateInfo { };
	ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ShaderModuleCreateInfo.codeSize = ShaderBinary.size();
	ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32*>(ShaderBinary.data());

	VkShaderModule ShaderModule;
	if (vkCreateShaderModule(m_VulkanLogicalDevice, &ShaderModuleCreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Failed to create a VulkanShaderModule");
	}
	
	return ShaderModule;
}

void VulkanAPI::CreateVulkanLogicalDevice()
{
	VulkanQueueFamilyIndices QueueFamilyIndices = GetDeviceQueueFamilies(m_VulkanPhysicalDevice);
	if (!QueueFamilyIndices.WasSet())
	{
		UNICA_LOG(Fatal, __FUNCTION__, "No support for graphics and image presentation queues");
		return;
	}

	std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
	std::set<uint32> UniqueQueueFamilies = { QueueFamilyIndices.GetGraphicsFamily().value(), QueueFamilyIndices.GetPresentImagesFamily().value() };
	
	const float QueuePriority = 1.f;
	for (const uint32 UniqueQueueFamily : UniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo QueueCreateInfo { };
		QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		QueueCreateInfo.queueFamilyIndex = UniqueQueueFamily;
		QueueCreateInfo.queueCount = 1;
		QueueCreateInfo.pQueuePriorities = &QueuePriority;
		QueueCreateInfos.push_back(QueueCreateInfo);
	}

	VkPhysicalDeviceFeatures DeviceFeatures { };

	VkDeviceCreateInfo DeviceCreateInfo { };
	DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32>(QueueCreateInfos.size());
	DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
	DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;

	DeviceCreateInfo.enabledExtensionCount = static_cast<uint32>(m_RequiredDeviceExtensions.size());
	DeviceCreateInfo.ppEnabledExtensionNames = m_RequiredDeviceExtensions.data();

	if (m_bValidationLayersEnabled)
	{
		DeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_RequestedValidationLayers.size());
		DeviceCreateInfo.ppEnabledLayerNames = m_RequestedValidationLayers.data();
	}
	else
	{
		DeviceCreateInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_VulkanPhysicalDevice, &DeviceCreateInfo, nullptr, &m_VulkanLogicalDevice) != VK_SUCCESS)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Couldn't create a VulkanLogicalDevice");
		return;
	}
	
	vkGetDeviceQueue(m_VulkanLogicalDevice, QueueFamilyIndices.GetGraphicsFamily().value(), 0, &m_VulkanGraphicsQueue);
	vkGetDeviceQueue(m_VulkanLogicalDevice, QueueFamilyIndices.GetPresentImagesFamily().value(), 0, &m_VulkanPresentImagesQueue);
}

void VulkanAPI::CreateVulkanDebugMessenger()
{
	if (m_bValidationLayersEnabled)
	{
		return;
	}

	VkDebugUtilsMessengerCreateInfoEXT VulkanCreateInfo;
	PopulateVulkanDebugMessengerInfo(VulkanCreateInfo);

	if (CreateVulkanDebugUtilsMessenger(m_VulkanInstance, &VulkanCreateInfo, nullptr, &m_VulkanDebugMessenger) != VK_SUCCESS)
	{
		UNICA_LOG(Error, __FUNCTION__, "Couldn't setup VulkanDebugMessenger");
	}
}

void VulkanAPI::CreateVulkanWindowSurface()
{
	if (glfwCreateWindowSurface(m_VulkanInstance, m_GlfwRenderWindow->GetGlfwWindow(), nullptr, &m_VulkanWindowSurface))
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Failed to create VulkanWindowSurface");
		return;
	}
}

void VulkanAPI::PopulateVulkanDebugMessengerInfo(VkDebugUtilsMessengerCreateInfoEXT& VulkanCreateInfo)
{
	VulkanCreateInfo = { };
	VulkanCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	VulkanCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	VulkanCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	VulkanCreateInfo.pfnUserCallback = VulkanDebugCallback;
}

void VulkanAPI::AddRequiredExtensions(VkInstanceCreateInfo& VulkanCreateInfo, std::vector<const char*>& RequiredExtensions)
{
#ifdef __APPLE__
    RequiredExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    RequiredExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif // __APPLE__

	uint32 AvailableInstanceExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &AvailableInstanceExtensionCount, nullptr);
	std::vector<VkExtensionProperties> AvailableInstanceExtensions(AvailableInstanceExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &AvailableInstanceExtensionCount, AvailableInstanceExtensions.data());

	bool bAllExtensionsFound = true;
	for (const char* RequiredExtensionName : RequiredExtensions)
	{
		bool bWasExtensionFound = false;
		for (const VkExtensionProperties& AvailableExtension : AvailableInstanceExtensions)
		{
			if (strcmp(RequiredExtensionName, AvailableExtension.extensionName) == 0)
			{
				bWasExtensionFound = true;
				break;
			}
		}
		if (!bWasExtensionFound)
		{
			UNICA_LOG(Error, __FUNCTION__, fmt::format("Graphic instance extension \"{}\" not found", RequiredExtensionName));
			bAllExtensionsFound = false;
		}
	}
	if (!bAllExtensionsFound)
	{
		UNICA_LOG(Fatal, __FUNCTION__, "Not all required instance extensions found");
		return;
	}

	VulkanCreateInfo.enabledExtensionCount = static_cast<uint32>(RequiredExtensions.size());
	VulkanCreateInfo.ppEnabledExtensionNames = RequiredExtensions.data();
}

void VulkanAPI::AddValidationLayers(VkInstanceCreateInfo& VulkanCreateInfo, VkDebugUtilsMessengerCreateInfoEXT& VulkanDebugCreateInfo)
{
	if (!m_bValidationLayersEnabled)
	{
		return;
	}

	uint32 LayerCount;
	vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);

	std::vector<VkLayerProperties> AvailableLayers(LayerCount);
	vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());

	bool bAllLayersFound = true;
	for (const char* RequestedLayerName : m_RequestedValidationLayers)
	{
		bool bWasLayerFound = false;
		for (const VkLayerProperties& AvailableLayer : AvailableLayers)
		{
			if (strcmp(RequestedLayerName, AvailableLayer.layerName) == 0)
			{
				bWasLayerFound = true;
				break;
			}
		}
		if (!bWasLayerFound)
		{
			UNICA_LOG(Error, __FUNCTION__, fmt::format("VulkanValidationLayer \"{}\" not found", RequestedLayerName));
			bAllLayersFound = false;
		}
	}
	if (!bAllLayersFound)
	{
		UNICA_LOG(Error, __FUNCTION__, "Won't enable VulkanValidationLayers since not all of them are available");
		return;
	}

	VulkanCreateInfo.enabledLayerCount = static_cast<uint32>(m_RequestedValidationLayers.size());
	VulkanCreateInfo.ppEnabledLayerNames = m_RequestedValidationLayers.data();

	PopulateVulkanDebugMessengerInfo(VulkanDebugCreateInfo);
	VulkanCreateInfo.pNext = &VulkanDebugCreateInfo;
}

VkResult VulkanAPI::CreateVulkanDebugUtilsMessenger(VkInstance VulkanInstance, const VkDebugUtilsMessengerCreateInfoEXT* VulkanCreateInfo, const VkAllocationCallbacks* VulkanAllocator, VkDebugUtilsMessengerEXT* VulkanDebugMessenger)
{
	const auto Func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(VulkanInstance, "vkCreateDebugUtilsMessengerEXT"));
	if (Func != nullptr)
	{
		return Func(VulkanInstance, VulkanCreateInfo, VulkanAllocator, VulkanDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanAPI::DestroyVulkanDebugUtilsMessengerEXT(VkInstance VulkanInstance, VkDebugUtilsMessengerEXT VulkanDebugMessenger, const VkAllocationCallbacks* VulkanAllocator)
{
	const auto Func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(VulkanInstance, "vkDestroyDebugUtilsMessengerEXT"));
	if (Func != nullptr)
	{
		Func(VulkanInstance, VulkanDebugMessenger, VulkanAllocator);
	}
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanAPI::VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity, VkDebugUtilsMessageTypeFlagsEXT MessageType, const VkDebugUtilsMessengerCallbackDataEXT* CallbackData, void* UserData)
{
	LogLevel LogLvl = Log;

	if ((MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		LogLvl = Warning;
	}
	else if ((MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LogLvl = Error;
	}

	UNICA_LOG(LogLvl, __FUNCTION__, CallbackData->pMessage);
	return VK_FALSE;
}

void VulkanAPI::Shutdown()
{
	vkDestroyPipeline(m_VulkanLogicalDevice, m_VulkanGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_VulkanLogicalDevice, m_VulkanPipelineLayout, nullptr);
	vkDestroyRenderPass(m_VulkanLogicalDevice, m_VulkanRenderPass, nullptr);
	for (const VkImageView& SwapChainImageView : m_VulkanSwapChainImageViews)
	{
		vkDestroyImageView(m_VulkanLogicalDevice, SwapChainImageView, nullptr);
	}
	vkDestroySwapchainKHR(m_VulkanLogicalDevice, m_VulkanSwapChain, nullptr);
	vkDestroyDevice(m_VulkanLogicalDevice, nullptr);
	vkDestroySurfaceKHR(m_VulkanInstance, m_VulkanWindowSurface, nullptr);
	if (m_bValidationLayersEnabled)
	{
		DestroyVulkanDebugUtilsMessengerEXT(m_VulkanInstance, m_VulkanDebugMessenger, nullptr);
	}
	vkDestroyInstance(m_VulkanInstance, nullptr);
	UNICA_LOG(Log, __FUNCTION__, "Vulkan instance has been destroyed");
}

