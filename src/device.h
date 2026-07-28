#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// 有device.cpp 选择物理设备，创建逻辑设备，队列，检查vulkan扩展、设备功能、交换链是否支持等
class Device{
public:
    Device(){};
    ~Device(){
        vkDestroyDevice(device, nullptr);
        
    };
    VkDevice getDevice(){
        return device;
    }
    VkPhysicalDevice getPhysicalDevice(){
        return physicalDevice;
    }
    void pickPhysicalDevice();
    void createLogicalDevice();
    
private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
};