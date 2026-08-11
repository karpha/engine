#pragma once
#include "device.h"
#include "window.h"
#include "instance.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class SwapChain{
public:
    SwapChain(Device* pDevice){
        device = pDevice;
    };
    ~SwapChain(){
        destroySwapChain();
    };

    // 销毁交换链并置空，供 recreateSwapChain 和析构函数调用（幂等）
    void destroySwapChain(){
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device->getDevice(), swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
    };

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    // void setPhysicalDev(VkPhysicalDevice physicalDev){
    //     physicalDevice = physicalDev;
    // }
    // void setDev(VkDevice dev){
    //     device = dev;
    // }
    VkSwapchainKHR getSwapchain(){
        return swapchain;
    }
    
    void createSwapChain(Device* pDevice, 
        VkPhysicalDevice physicalDevice, 
        Window* pWindow, 
        Instance* pInstance);
    void createImageViews(Device* pDevice);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, Device* pDevice);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* pWindow);     // 参数修改为Window* pWindow
    
    std::vector<VkImage> getSwapchainImages(){
        return swapChainImages;
    }
    VkFormat getSwapchainImageFormat(){
        return swapChainImageFormat;
    }
    VkExtent2D getSwapchainExtent(){
        return swapChainExtent;
    }
    std::vector<VkImageView>& getSwapchainImageViews(){
        return swapChainImageViews;
    }
    std::vector<VkFramebuffer>& getSwapchainFrameBuffers(){
        return swapChainFramebuffers;
    }

private:
    Device* device;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

};