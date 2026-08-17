#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// #define GLM_ENABLE_EXPERIMENTAL
// #include <glm/gtx/hash.hpp>
// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>

// #define TINYOBJLOADER_IMPLEMENTATION
// #include <tiny_obj_loader.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>
#include <memory>   // 使用instance的修改

#include "instance.h"
#include "window.h"
#include "device.h"
#include "swapchain.h"
#include "renderpass.h"
#include "buffer.h"
#include "command.h"
#include "descriptor.h"
#include "texture.h"
#include "loadModel.h"
#include "pipeline.h"
#include "other.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

class HelloTriangleApplication {
    public:
    void run() {
        std::cout << "run : \n";
        initVulkan();
        mainLoop();
        // cleanup();
    }

private:
    // RAII：直接成员，按声明顺序构造（windowName -> window -> instance）
    std::string windowName = "renderer";        // 使用构造函数声明类对象时，会首先直接调用构造函数，
    // 但是如果有一些创建资源的函数需要有前后顺序要求，就需要注意函数调用顺序
    Window window{800, 600, windowName};
    Instance instance{"Hello Triangle", "No Engine", true, window};
    Device device{instance};
    SwapChain swapchain{&device, &window,&instance};
    
    RenderPass renderpass{&device, &swapchain};
    Descriptor descriptor{&device};
    Pipeline pipeline{&device, &descriptor, &renderpass};

    Command command{&device, &descriptor};

    Texture texture{&device, &command};
    Other other{&device, &swapchain, &texture, &renderpass, &descriptor};
    Buffer buffer{&device, &texture, &swapchain, &renderpass, &command, &descriptor};
    // std::unique_ptr<RenderPass> pRenderPass;
    LoadModel loadmodel{&buffer};
    // std::unique_ptr<Buffer> pBuffer;
    // std::unique_ptr<Command> pCommand;
    // std::unique_ptr<Descriptor> pDescriptor;
    // std::unique_ptr<Texture> pTexture;

    // std::unique_ptr<LoadModel> pLoadModel;
    // std::unique_ptr<Pipeline> pPipeLine;
    // std::unique_ptr<Other> pOther;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    void initVulkan() {
        std::cout << "pInstance\n";
        std::cout << "pDevice\n";
        std::cout << "GraphicsQueue\n";
        std::cout << "pswapchain1\n";
        
        // pRenderPass = std::make_unique<RenderPass>(&device, &swapchain);
        // pRenderPass->createRenderPass();
        std::cout << "pRenderpass\n";
        
        // pBuffer = std::make_unique<Buffer>(&device);
        // pCommand = std::make_unique<Command>(&device);
        // pTexture = std::make_unique<Texture>(&device, pBuffer.get(), pCommand.get());
        // pDescriptor = std::make_unique<Descriptor>(&device, pTexture.get());
        // pDescriptor->setBuffer(pBuffer.get());
        // pBuffer->bufferInit(pTexture.get(), &swapchain, &renderpass, pCommand.get(), pDescriptor.get());
        std::cout << "pDescriptor\n";

        // pLoadModel = std::make_unique<LoadModel>(pBuffer.get());
        
        // pCommand->setDescriptor(pDescriptor.get());
        // pCommand->setGraphicsQueue(device.getGraphicsQueue());
        
        // pDescriptor->createDescriptorSetLayout();

        // pPipeLine = std::make_unique<Pipeline>(&device, pDescriptor.get(), &renderpass);
        // pPipeLine->createGraphicsPipeline();
        
        // pCommand->createCommandPool();

        // pOther = std::make_unique<Other>(&device, &swapchain, pTexture.get(), &renderpass, pDescriptor.get());
        // pOther->createColorResources();
        // pOther->createDepthResources();
        
        // pBuffer->createFramebuffers();
        command.setGraphicsQueue(device.getGraphicsQueue());   // 必须在使用 command 执行提交前设置队列
        texture.init(&buffer);
        std::cout << "before createTextureImage\n";
        texture.createTextureImage();
        std::cout << "after createTextureImage\n";
        // texture.
        // pTexture->createTextureImage();
        texture.createTextureImageView();
        std::cout << "after createTextureImageView\n";
        texture.createTextureSampler();
        std::cout << "after createTextureSampler\n";

        // pLoadModel->loadModel();
        
        // 添加buffer init
        buffer.createVertexBuffer();
        // pBuffer->createVertexBuffer();
        buffer.createIndexBuffer();
        buffer.createUniformBuffers();
        // 
        // 这里使用descriptor init，传入texture， buffer
        descriptor.init(&buffer, &texture);
        descriptor.createDescriptorPool();
        descriptor.createDescriptorSets();
        
        // pCommand->createCommandBuffers();
        command.createCommandBuffers();
        // pOther->createSyncObjects();
        other.createSyncObjects();
        imageAvailableSemaphores = other.getImageAvailableSemaphores();
        renderFinishedSemaphores = other.getRenderFinishedSemaphores();
        inFlightFences = other.getInFlightFences();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window.getWindow())) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device.getDevice());
    }

    void cleanupSwapChain() {       // for window resized
        vkDestroyImageView(device.getDevice(), texture.getDepthImageView(), nullptr);
        vkDestroyImage(device.getDevice(), texture.getDepthImage(), nullptr);
        vkFreeMemory(device.getDevice(), texture.getDepthImageMemory(), nullptr);
        texture.setDepthImageView(VK_NULL_HANDLE);
        texture.setDepthImage(VK_NULL_HANDLE);
        texture.setDepthImageMemory(VK_NULL_HANDLE);

        vkDestroyImageView(device.getDevice(), texture.getColorImageView(), nullptr);
        vkDestroyImage(device.getDevice(), texture.getColorImage(), nullptr);
        vkFreeMemory(device.getDevice(), texture.getColorImageMemory(), nullptr);
        texture.setColorImageView(VK_NULL_HANDLE);
        texture.setColorImage(VK_NULL_HANDLE);
        texture.setColorImageMemory(VK_NULL_HANDLE);

        for (auto framebuffer : swapchain.getSwapchainFrameBuffers()) {
            vkDestroyFramebuffer(device.getDevice(), framebuffer, nullptr);
        }
        swapchain.getSwapchainFrameBuffers().clear();

        for (auto imageView : swapchain.getSwapchainImageViews()) {
            vkDestroyImageView(device.getDevice(), imageView, nullptr);
        }
        swapchain.getSwapchainImageViews().clear();
        swapchain.destroySwapChain();
    }

    void recreateSwapChain() {      // for window resized
        int width = 0, height = 0;
        glfwGetFramebufferSize(window.getWindow(), &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window.getWindow(), &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device.getDevice());

        cleanupSwapChain();

        swapchain.createSwapChain(&device,device.getPhysicalDevice(),&window,&instance);
        swapchain.createImageViews(&device);
        // createColorResources();
        other.createColorResources();
        other.createDepthResources();
        buffer.createFramebuffers();
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderpass.getRenderpass();
        renderPassInfo.framebuffer = swapchain.getSwapchainFrameBuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchain.getSwapchainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsPipeline());

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) swapchain.getSwapchainExtent().width;
            viewport.height = (float) swapchain.getSwapchainExtent().height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapchain.getSwapchainExtent();
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            VkBuffer vertexBuffers[] = {buffer.getVertexBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(commandBuffer, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipelineLayout(), 0, 1, &(descriptor.getDescriptorSets())[currentFrame], 0, nullptr);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(buffer.getIndices().size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapchain.getSwapchainExtent().width / (float) swapchain.getSwapchainExtent().height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(buffer.getUniformBuffersMapped()[currentImage], &ubo, sizeof(ubo));
    }

    void drawFrame() {
        vkWaitForFences(device.getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device.getDevice(), swapchain.getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {       // for window resized
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(currentFrame);

        vkResetFences(device.getDevice(), 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(command.getCommandBuffers()[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
        recordCommandBuffer(command.getCommandBuffers()[currentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &(command.getCommandBuffers())[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapchain.getSwapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(device.getPresentQueue() , &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
};

int main() {
    try {
        std::cout << "main: try: \n";
        HelloTriangleApplication app;   // 对象在 try 内构造，构造阶段的异常可被捕获并打印
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
