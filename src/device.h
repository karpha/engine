#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <optional>
#include <vector>

#include "instance.h"
#include <iostream>

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// 有device.cpp 选择物理设备，创建逻辑设备，队列，检查vulkan扩展、设备功能、交换链是否支持等
class Device{
public:

    Device(Instance& inst);
    ~Device(){
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
        }
        std::cout << "\tdevice : destructor\n";
    }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    void setPhysicalDevice(VkPhysicalDevice phyDev){
        physicalDevice = phyDev;
    }
    VkDevice getDevice(){
        return device;
    }
    VkPhysicalDevice getPhysicalDevice(){
        return physicalDevice;
    }
    VkSampleCountFlagBits getMsaaSamples(){
        return msaaSamples;
    }
    VkQueue getGraphicsQueue() const {
        return graphicsQueue;
    }
    VkQueue getPresentQueue() const {
        return presentQueue;
    }
    void setMsaaSamples(VkSampleCountFlagBits samples){
        msaaSamples = samples;
    }
    void pickPhysicalDevice(Instance& instance);
    void createLogicalDevice();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    VkSampleCountFlagBits getMaxUsableSampleCount();

private:
    Instance* instance;
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
};
