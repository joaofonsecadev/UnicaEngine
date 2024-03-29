// 2022-2023 Copyright joaofonseca.dev, All Rights Reserved.

#include "VulkanWindowSurface.h"

#include <SDL_vulkan.h>

#include "Logging/Logger.h"
#include "Renderer/Vulkan/VulkanInterface.h"

void VulkanWindowSurface::Init()
{
    if (SDL_Vulkan_CreateSurface(m_OwningVulkanAPI->GetSdlRenderWindow()->GetSdlWindow(), m_OwningVulkanAPI->GetVulkanInstance()->GetVulkanObject(), &m_VulkanObject) == SDL_FALSE)
    {
        UNICA_LOG(spdlog::level::critical, "Failed to create SDLVulkanWindowSurface");
    }
    UNICA_LOG_TRACE("VulkanWindowSurface created");
}

void VulkanWindowSurface::Destroy()
{
    UNICA_LOG_TRACE("Destroying VulkanWindowSurface");
    vkDestroySurfaceKHR(m_OwningVulkanAPI->GetVulkanInstance()->GetVulkanObject(), m_VulkanObject, nullptr);
}
