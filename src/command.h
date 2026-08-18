#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "device.h"
#include <iostream>

class Descriptor;

class Command{
public:
    Command(Device* dev, Descriptor* descrpt);
    ~Command(){
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device->getDevice(), commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        std::cout << "\tcommand: destructor\n";
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
    std::vector<VkCommandBuffer> commandBuffers;       // 销毁命令池时，从该池分配的命令缓冲区会被自动释放
    VkQueue graphicsQueue;      // 不是独立分配的资源，属于逻辑设备的一部分，没有对应的vkdestroy函数，
    // 队列的声明周期由逻辑设备管理，当vkDestroyDevice时，队列随之销毁

    Device* device;
    Descriptor* descriptor;
};