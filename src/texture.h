#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "device.h"
// #include "buffer.h"
#include "command.h"

// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>
#include "thirdParty.h"
#include <iostream>

class Buffer;

class Texture{
public:
    Texture(Device* dev, Command* cmd){
        device = dev;
        // buffer = buff;
        command = cmd;
        std::cout << "\ttexture: constructor\n";
    }
    void init(Buffer* buff){
        buffer = buff;
    }    // init 中初始化buffer
    ~Texture(){
        if (textureImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device->getDevice(), textureImageView, nullptr);
            textureImageView = VK_NULL_HANDLE;
        }
        if (textureSampler != VK_NULL_HANDLE) {
            vkDestroySampler(device->getDevice(), textureSampler, nullptr);
            textureSampler = VK_NULL_HANDLE;
        }
        if (textureImage != VK_NULL_HANDLE) {
            vkDestroyImage(device->getDevice(), textureImage, nullptr);
            textureImage = VK_NULL_HANDLE;
        }
        if (textureImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->getDevice(), textureImageMemory, nullptr);
            textureImageMemory = VK_NULL_HANDLE;
        }

        // 销毁 MSAA color 资源
        if (colorImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device->getDevice(), colorImageView, nullptr);
            colorImageView = VK_NULL_HANDLE;
        }
        if (colorImage != VK_NULL_HANDLE) {
            vkDestroyImage(device->getDevice(), colorImage, nullptr);
            colorImage = VK_NULL_HANDLE;
        }
        if (colorImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->getDevice(), colorImageMemory, nullptr);
            colorImageMemory = VK_NULL_HANDLE;
        }

        // 销毁 depth 资源
        if (depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device->getDevice(), depthImageView, nullptr);
            depthImageView = VK_NULL_HANDLE;
        }
        if (depthImage != VK_NULL_HANDLE) {
            vkDestroyImage(device->getDevice(), depthImage, nullptr);
            depthImage = VK_NULL_HANDLE;
        }
        if (depthImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->getDevice(), depthImageMemory, nullptr);
            depthImageMemory = VK_NULL_HANDLE;
        }
        std::cout << "\ttexture: destructor\n";
    }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    VkImage getTextureImage(){
        return textureImage;
    }
    VkDeviceMemory getTextureImageMemory(){
        return textureImageMemory;
    }
    VkImageView getTextureImageView(){
        return textureImageView;
    }
    VkSampler getTextureSampler(){
        return textureSampler;
    }
    uint32_t getMipLevels(){
        return mipLevels;
    }
    VkImage getColorImage(){
        return colorImage;
    }
    VkDeviceMemory getColorImageMemory(){
        return colorImageMemory;
    }
    VkImageView getColorImageView(){
        return colorImageView;
    }

    VkImage getDepthImage(){
        return depthImage;
    }
    VkDeviceMemory getDepthImageMemory(){
        return depthImageMemory;
    }
    VkImageView getDepthImageView(){
        return depthImageView;
    }

    void setColorImage(VkImage img) { colorImage = img; }
    void setColorImageMemory(VkDeviceMemory mem) { colorImageMemory = mem; }
    void setColorImageView(VkImageView view) { colorImageView = view; }
    void setDepthImage(VkImage img) { depthImage = img; }
    void setDepthImageMemory(VkDeviceMemory mem) { depthImageMemory = mem; }
    void setDepthImageView(VkImageView view) { depthImageView = view; }
    
    void createTextureImage();
    void createTextureImageView();
    void createTextureSampler();
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    void createImage(
        uint32_t width, 
        uint32_t height, 
        uint32_t mipLevels, 
        VkSampleCountFlagBits numSamples, 
        VkFormat format, 
        VkImageTiling tiling, 
        VkImageUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkImage& image, 
        VkDeviceMemory& imageMemory);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

private:
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;
    const std::string TEXTURE_PATH = "textures/viking_room.png";
    uint32_t mipLevels = 1;

    VkImage colorImage = VK_NULL_HANDLE;
    VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
    VkImageView colorImageView = VK_NULL_HANDLE;

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    Device* device;
    Buffer* buffer;
    Command* command;
};