#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "device.h"
#include "swapchain.h"

class RenderPass{
public:
    RenderPass( Device* dev, SwapChain* sw){
        device = dev;
        swapchain = sw;
    }
    ~RenderPass(){
        if (renderpass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device->getDevice(),renderpass,nullptr);
            renderpass = VK_NULL_HANDLE;
        }
    }

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    VkFormat findDepthFormat(Device* device);
    void createRenderPass();
    VkRenderPass getRenderpass(){
        return renderpass;
    }
private:
    VkRenderPass renderpass = VK_NULL_HANDLE;
    
    Device* device;
    SwapChain* swapchain;
};