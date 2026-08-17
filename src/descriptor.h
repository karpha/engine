#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "device.h"
#include "texture.h"

class Buffer;

// 需要TEXTURE 和 unifor BUFFER

class Descriptor{
public:
    Descriptor(Device* dev);
    void setBuffer(Buffer* buf){
        buffer = buf;
    }
    ~Descriptor(){      // 析构函数需要device
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
    void init(Buffer* buff, Texture* tex){
        buffer = buff;
        texture = tex;
    }

    void createDescriptorSetLayout();       // 1
    void createDescriptorPool();     // descriptor  2
    void createDescriptorSets();        // 3
private:
    Device* device;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;        // 与command buffer同理，是从descriptor pool分配的，不是通过vkcreate函数创建的
    // 没有对应的vkDestroyDescriptorSets函数，当销毁descriptor pool时，该pool分配的descriptor set会自动释放
    const int MAX_FRAMES_IN_FLIGHT = 2;

    Buffer* buffer;
    Texture* texture;
};