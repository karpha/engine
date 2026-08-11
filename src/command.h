#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "device.h"

class Descriptor;

class Command{
public:
    Command(Device* dev){
        device = dev;
    }
    ~Command(){
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device->getDevice(), commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
    }
    void setDescriptor(Descriptor* des){
        descriptor = des;
    }

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void createCommandPool();
    void createCommandBuffers();

    VkCommandPool getCommandPool(){
        return commandPool;
    }
    std::vector<VkCommandBuffer>& getCommandBuffers(){
        return commandBuffers;
    }
    void setGraphicsQueue(VkQueue queue){
        graphicsQueue = queue;
    }

private:
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkQueue graphicsQueue;

    Device* device;
    Descriptor* descriptor;
};