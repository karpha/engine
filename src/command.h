#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "device.h"

class Command{
public:
    Command(Device* dev){
        device = dev;
    }
    ~Command(){
        vkDestroyCommandPool(device->getDevice(), commandPool, nullptr);
    }

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void createCommandPool();

private:
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkQueue graphicsQueue;

    Device* device;
};