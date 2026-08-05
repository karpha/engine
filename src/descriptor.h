#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "device.h"
#include "texture.h"

class Buffer;

// 需要TEXTURE 和 unifor BUFFER

class Descriptor{
public:
    Descriptor(Device* dev, Texture* txture){
        device = dev;
        texture = txture;
    }
    void setBuffer(Buffer* buf){
        buffer = buf;
    }
    ~Descriptor(){
        if (descriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device->getDevice(), descriptorSetLayout, nullptr);
        if (descriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device->getDevice(), descriptorPool, nullptr);
    }

    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;

    VkDescriptorSetLayout getDescriptorSetLayout(){
        return descriptorSetLayout;
    }
    VkDescriptorPool getDescriptorPool(){
        return descriptorPool;
    }
    std::vector<VkDescriptorSet>& getDescriptorSets(){
        return descriptorSets;
    }
    const int getMAX_FRAMES_IN_FLIGHT() const{
        return MAX_FRAMES_IN_FLIGHT;
    }

    void createDescriptorSetLayout();       // 1
    void createDescriptorPool();     // descriptor  2
    void createDescriptorSets();        // 3
private:
    Device* device;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    // VkDescriptorSet descriptorSet;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    Buffer* buffer;
    Texture* texture;
};