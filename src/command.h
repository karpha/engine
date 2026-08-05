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
        vkDestroyCommandPool(device->getDevice(), commandPool, nullptr);
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
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkQueue graphicsQueue;

    Device* device;
    Descriptor* descriptor;
};