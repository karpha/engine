#include "other.h"
#include <stdexcept>

#include "device.h"
#include "texture.h"
#include "swapchain.h"
#include "renderpass.h"
#include "descriptor.h" 


void Other::createColorResources() {
    VkFormat colorFormat = pSwapchain->getSwapchainImageFormat();
    VkImage colorImage = pTexture->getColorImage();
    VkDeviceMemory colorImageMemory = pTexture->getColorImageMemory();

    pTexture->createImage(pSwapchain->getSwapchainExtent().width, pSwapchain->getSwapchainExtent().height, 1, device->getMsaaSamples(), colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
    pTexture->setColorImage(colorImage);
    pTexture->setColorImageMemory(colorImageMemory);
    colorImageView = pSwapchain->createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, device);
    pTexture->setColorImageView(colorImageView);
}

void Other::createDepthResources() {
    VkFormat depthFormat = pRenderpass->findDepthFormat(device);
    VkImage depthImage = pTexture->getDepthImage();
    VkDeviceMemory depthImageMemory = pTexture->getDepthImageMemory();

    pTexture->createImage(pSwapchain->getSwapchainExtent().width, pSwapchain->getSwapchainExtent().height, 1, device->getMsaaSamples(), depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    pTexture->setDepthImage(depthImage);
    pTexture->setDepthImageMemory(depthImageMemory);
    depthImageView = pSwapchain->createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, device);
    pTexture->setDepthImageView(depthImageView);
}

void Other::createSyncObjects() {
    imageAvailableSemaphores.resize(pDescriptor->getMAX_FRAMES_IN_FLIGHT());
    renderFinishedSemaphores.resize(pDescriptor->getMAX_FRAMES_IN_FLIGHT());
    inFlightFences.resize(pDescriptor->getMAX_FRAMES_IN_FLIGHT());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < pDescriptor->getMAX_FRAMES_IN_FLIGHT(); i++) {
        if (vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}