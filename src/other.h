#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class SwapChain;
class Texture;
class RenderPass;
class Device;
class Descriptor;

class Other{
public:
    Other(Device* dev, SwapChain* swchain, Texture* tex, RenderPass* rp, Descriptor* descr){
        device = dev;
        pSwapchain = swchain;
        pTexture = tex;
        pRenderpass = rp;
        pDescriptor = descr;
    }
    ~Other();

    Other(const Other&) = delete;
    Other& operator=(const Other&) = delete;

    void createColorResources();
    void createDepthResources();
    void createSyncObjects();

    VkImageView getColorImageView(){
        return colorImageView;
    }
    VkImageView getDepthImageView(){
        return depthImageView;
    }

    std::vector<VkSemaphore> getImageAvailableSemaphores(){
        return imageAvailableSemaphores;
    }
    std::vector<VkSemaphore> getRenderFinishedSemaphores(){
        return renderFinishedSemaphores;
    }
    std::vector<VkFence> getInFlightFences(){
        return inFlightFences;
    }

private:
    VkImageView colorImageView = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    std::vector<VkSemaphore> imageAvailableSemaphores;      //使用vector容器定义的变量不需要手动释放，会自动管理内存与生命周期
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    Device* device = nullptr;
    SwapChain* pSwapchain = nullptr;
    Texture* pTexture = nullptr;
    RenderPass* pRenderpass = nullptr;
    Descriptor* pDescriptor = nullptr;

};