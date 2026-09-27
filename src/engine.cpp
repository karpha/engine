#include "engine.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cstring>
#include <stdexcept>
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
#include "camera.h"
#include "pbr.h"
#include "ImGui.h"


namespace {
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
EngineConfig checkedConfig(EngineConfig config) {
    if (config.width<=0 || config.height<=0 || config.frameLimit<0)
        throw std::invalid_argument("Invalid engine window dimensions or frame limit");
    return config;
}
void validateTransform(const glm::mat4& matrix) {
    for (int c=0;c<4;++c) for (int r=0;r<4;++r)
        if (!std::isfinite(matrix[c][r])) throw std::invalid_argument("Non-finite object transform");
    if (std::abs(glm::determinant(glm::mat3(matrix)))<=1e-12f ||
        matrix[0][3]!=0 || matrix[1][3]!=0 || matrix[2][3]!=0 || matrix[3][3]!=1)
        throw std::invalid_argument("Object transform must be affine and nonsingular");
}
}

struct Engine::Impl {
    EngineConfig config;
    Window window{config.width,config.height,config.title};
    Instance instance{"Rendering application","Vulkan Engine",true,window};
    Device device{instance};
    SwapChain swapchain{&device,&window,&instance};
    RenderPass renderpass{&device,&swapchain};
    Descriptor descriptor{&device};
    std::unique_ptr<Pipeline> pipeline;
    Command command{&device,&descriptor};
    Texture texture{&device,&command};
    Other other{&device,&swapchain,&texture,&renderpass,&descriptor};
    Buffer buffer{&device,&texture,&swapchain,&renderpass,&command,&descriptor};
    std::unique_ptr<LoadModel> loadmodel;
    PbrRenderer pbr{device,buffer,texture,renderpass,MAX_FRAMES_IN_FLIGHT};
    Camera camera;
    ImGuiForVulkan gui;
    bool usePBR;
    bool hasRun = false;
    ObjectId nextObject = 1;
    std::map<std::string, ModelHandle> models;
    std::map<ObjectId, RenderItem> objects;
    std::vector<VkSemaphore> imageAvailableSemaphores, presentationSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    explicit Impl(EngineConfig settings) : config(checkedConfig(std::move(settings))), usePBR(config.pbr) {}
    ~Impl() {
        vkDeviceWaitIdle(device.getDevice());
        for (auto semaphore : presentationSemaphores) vkDestroySemaphore(device.getDevice(),semaphore,nullptr);
        // Framebuffers reference attachments owned by Texture, which is destroyed first.
        for (auto framebuffer : swapchain.getSwapchainFrameBuffers())
            vkDestroyFramebuffer(device.getDevice(),framebuffer,nullptr);
        swapchain.getSwapchainFrameBuffers().clear();
    }
    void createPresentationSemaphores() {
        for (auto semaphore : presentationSemaphores) vkDestroySemaphore(device.getDevice(),semaphore,nullptr);
        presentationSemaphores.assign(swapchain.getSwapchainImageViews().size(),VK_NULL_HANDLE);
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (auto& semaphore : presentationSemaphores)
            if (vkCreateSemaphore(device.getDevice(),&info,nullptr,&semaphore)!=VK_SUCCESS)
                throw std::runtime_error("Failed to create presentation semaphore");
    }
    void initialize() {
        command.setGraphicsQueue(device.getGraphicsQueue());   // 必须在使用 command 执行提交前设置队列
        texture.init(&buffer);
        if (!usePBR) {
            pipeline = std::make_unique<Pipeline>(&device, &descriptor, &renderpass);
            loadmodel = std::make_unique<LoadModel>(&buffer);
            texture.createTextureImage();
            texture.createTextureImageView();
            texture.createTextureSampler();
            buffer.createVertexBuffer();
            buffer.createIndexBuffer();
            buffer.createUniformBuffers();
            descriptor.init(&buffer, &texture);
            descriptor.createDescriptorPool();
            descriptor.createDescriptorSets();
        }
        if (usePBR) pbr.initialize();
        command.createCommandBuffers();
        other.createSyncObjects();
        imageAvailableSemaphores = other.getImageAvailableSemaphores();
        createPresentationSemaphores();
        inFlightFences = other.getInFlightFences();
        camera.setupInputCallbacks(window.getWindow(),camera);
        gui.init(window.getWindow(), instance, device, swapchain, renderpass, MAX_FRAMES_IN_FLIGHT);
    }
    void cleanupSwapChain() {       // for window resized
        for (auto framebuffer : swapchain.getSwapchainFrameBuffers()) {
            vkDestroyFramebuffer(device.getDevice(), framebuffer, nullptr);
        }
        swapchain.getSwapchainFrameBuffers().clear();


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
            if (glfwWindowShouldClose(window.getWindow())) return;
            glfwGetFramebufferSize(window.getWindow(), &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device.getDevice());

        const auto previousFormat = swapchain.getSwapchainImageFormat();
        cleanupSwapChain();

        swapchain.createSwapChain(&device,device.getPhysicalDevice(),&window,&instance);
        if (swapchain.getSwapchainImageFormat() != previousFormat)
            throw std::runtime_error("Swapchain format changed; restart to recreate render pipelines");
        swapchain.createImageViews(&device);
        // createColorResources();
        other.createColorResources();
        other.createDepthResources();
        buffer.createFramebuffers();
        createPresentationSemaphores();
        window.setFrameBufferResized(false);
        gui.onSwapChainRecreated();
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        gui.prepareFrame(commandBuffer, currentFrame);

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

        if (usePBR) {
            pbr.draw(commandBuffer, currentFrame, swapchain.getSwapchainExtent());
        } else{

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getGraphicsPipeline());

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

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &(descriptor.getDescriptorSets())[currentFrame], 0, nullptr);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(buffer.getIndices().size()), 1, 0, 0, 0);

        }
        gui.drawFrame(commandBuffer);
        vkCmdEndRenderPass(commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void updateUniformBuffer(uint32_t currentImage, const Camera& camera) {
        UniformBufferObject ubo{};
        // The legacy OBJ uses Z-up; the engine scene uses Y-up.
        ubo.model = glm::rotate(glm::mat4(1),glm::radians(-90.0f),glm::vec3(1,0,0));
        ubo.view = camera.getViewMatrix();
        const auto extent = swapchain.getSwapchainExtent();
        ubo.proj = camera.getProjectionMatrix(float(extent.width)/float(extent.height));
        ubo.proj[1][1] *= -1;
        std::memcpy(buffer.getUniformBuffersMapped()[currentImage],&ubo,sizeof(ubo));
    }

    void drawFrame() {
        if (vkWaitForFences(device.getDevice(),1,&inFlightFences[currentFrame],VK_TRUE,UINT64_MAX)!=VK_SUCCESS)
            throw std::runtime_error("Failed to wait for frame fence");

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device.getDevice(), swapchain.getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {       // for window resized
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        gui.newFrame(camera,usePBR ? &pbr : nullptr);
        if (usePBR) {
            std::vector<RenderItem> items;
            items.reserve(objects.size());
            for (const auto& [id,item] : objects) items.push_back(item);
            pbr.updateFrame(currentFrame,items,camera,swapchain.getSwapchainExtent(),swapchain.getSwapchainImageFormat());
        }
        else updateUniformBuffer(currentFrame, camera);

        if (vkResetFences(device.getDevice(),1,&inFlightFences[currentFrame])!=VK_SUCCESS)
            throw std::runtime_error("Failed to reset frame fence");

        if (vkResetCommandBuffer(command.getCommandBuffers()[currentFrame],0)!=VK_SUCCESS)
            throw std::runtime_error("Failed to reset command buffer");
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

        VkSemaphore signalSemaphores[] = {presentationSemaphores[imageIndex]};
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

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.getFrameBufferResized()) {
            window.setFrameBufferResized(false);
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
};
Engine::Engine(EngineConfig config) : impl(std::make_unique<Impl>(std::move(config))) { impl->initialize(); }
Engine::~Engine() = default;
ModelHandle Engine::loadModel(const std::string& path) {
    if (!impl->usePBR) throw std::logic_error("Model objects require the PBR rendering path");
    const auto key=std::filesystem::canonical(path).generic_string();
    if (auto it=impl->models.find(key); it!=impl->models.end()) return it->second;
    // Upload helpers reuse the graphics queue and command pool. Loading is synchronous.
    if (vkDeviceWaitIdle(impl->device.getDevice())!=VK_SUCCESS) throw std::runtime_error("GPU wait failed");
    ModelHandle model{impl->pbr.loadModel(key)};
    impl->models.emplace(key,model);
    return model;
}
glm::mat4 Engine::fitTransform(ModelHandle handle, glm::vec3 center, float size) const {
    const auto bounds=impl->pbr.modelBounds(handle.value);
    const auto extent=bounds.maximum-bounds.minimum;
    const float dimension=std::max({extent.x,extent.y,extent.z});
    if (!std::isfinite(size) || size<=0 || !std::isfinite(dimension) || dimension<=1e-8f)
        throw std::invalid_argument("Invalid model fit size or bounds");
    const auto transform=glm::translate(glm::mat4(1),center)
        *glm::scale(glm::mat4(1),glm::vec3(size/dimension))
        *glm::translate(glm::mat4(1),-(bounds.minimum+bounds.maximum)*0.5f);
    validateTransform(transform);
    return transform;
}
Engine::ObjectId Engine::createObject(ModelHandle model, const glm::mat4& transform) {
    impl->pbr.modelBounds(model.value); // Reject invalid resource handles before insertion.
    validateTransform(transform);
    if (impl->nextObject==std::numeric_limits<ObjectId>::max()) throw std::overflow_error("Object IDs exhausted");
    const auto id=impl->nextObject++;
    impl->objects.emplace(id,RenderItem{model,transform});
    return id;
}
void Engine::setTransform(ObjectId object, const glm::mat4& transform) {
    validateTransform(transform);
    impl->objects.at(object).worldTransform=transform;
}
void Engine::removeObject(ObjectId object) {
    if (!impl->objects.erase(object)) throw std::out_of_range("Unknown scene object");
}
void Engine::clearObjects() { impl->objects.clear(); }
size_t Engine::objectCount() const { return impl->objects.size(); }
size_t Engine::modelCount() const { return impl->models.size(); }
void Engine::setLight(uint32_t index, glm::vec3 position, glm::vec3 intensity) {
    for (int i=0;i<3;++i)
        if (!std::isfinite(position[i]) || !std::isfinite(intensity[i]) || intensity[i]<0)
            throw std::invalid_argument("Invalid point light");
    impl->pbr.setLight(index,position,intensity);
}
void Engine::setExposure(float exposure) { impl->pbr.setExposure(exposure); }
Camera& Engine::camera() { return impl->camera; }
void Engine::requestClose() { glfwSetWindowShouldClose(impl->window.getWindow(),GLFW_TRUE); }
void Engine::resizeWindow(int width, int height) {
    if (width<=0 || height<=0) throw std::invalid_argument("Window dimensions must be positive");
    glfwSetWindowSize(impl->window.getWindow(),width,height);
}
void Engine::run(const Update& update) {
    if (impl->hasRun) throw std::logic_error("Engine::run may only be called once");
    impl->hasRun=true;
    double previous=glfwGetTime();
    int frames=0;
    try {
        while (!glfwWindowShouldClose(impl->window.getWindow())) {
            glfwPollEvents();
            if (glfwWindowShouldClose(impl->window.getWindow())) break;
            const double now=glfwGetTime();
            const float dt=float(std::clamp(now-previous,0.0,0.1));
            previous=now;
            impl->camera.processInput(impl->window.getWindow(),impl->camera,dt);
            if (update) update(*this,dt);
            if (glfwWindowShouldClose(impl->window.getWindow())) break;
            impl->drawFrame();
            if (impl->config.frameLimit>0 && ++frames>=impl->config.frameLimit) requestClose();
        }
        if (vkDeviceWaitIdle(impl->device.getDevice())!=VK_SUCCESS) throw std::runtime_error("GPU wait failed");
    } catch (...) {
        vkDeviceWaitIdle(impl->device.getDevice());
        throw;
    }
}
