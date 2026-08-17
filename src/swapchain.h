#pragma once
#include "device.h"
#include "window.h"
#include "instance.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

class SwapChain{
public:
    SwapChain(Device* pDevice, Window* window, Instance* instance);
    ~SwapChain(){
        // 先销毁 framebuffers 与 image views，再销毁 swapchain（在 Device 销毁之前，设备仍有效）
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device->getDevice(), framebuffer, nullptr);
        }
        swapChainFramebuffers.clear();

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device->getDevice(), imageView, nullptr);
        }
        swapChainImageViews.clear();

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
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;       //  交换链自身分配、拥有， 获取时通过vkGetSwapchainImagesKHR函数获取而不是使用vkcreate创建
    VkFormat swapChainImageFormat;      //  只是一个枚举值，不需要销毁
    VkExtent2D swapChainExtent;         //  只是一个结构体数据，不需要销毁
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    Device* device;
};