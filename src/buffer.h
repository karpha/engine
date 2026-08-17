#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <glm/glm.hpp>

#include "device.h"
// #include "texture.h"
#include "swapchain.h"
#include "renderpass.h"
#include "command.h"
#include <array>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <iostream>

class Texture;

class Descriptor;   // 在头文件中只 前向声明， 在cpp中才真正包含

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}


class Buffer{
public:
    Buffer(Device* dev, Texture* txture, SwapChain* sw, RenderPass* renderp, Command* cmd, Descriptor* desc){
        device = dev;
        texture = txture;
        swapchain = sw;
        renderpass = renderp;
        command = cmd;
        descrpt = desc;
        createFramebuffers();
        std::cout << "buffer: constructor\n";
    }
    ~Buffer(){
        // 先销毁 uniform buffers 及其内存
        for (size_t i = 0; i < uniformBuffers.size(); i++) {        // vulkan 类型变量，先使用vkdestroy释放资源，之后再用vector的clear()函数
            vkDestroyBuffer(device->getDevice(), uniformBuffers[i], nullptr);
            vkFreeMemory(device->getDevice(), uniformBuffersMemory[i], nullptr);
        }
        uniformBuffers.clear();
        uniformBuffersMemory.clear();
        uniformBuffersMapped.clear();

        // 再销毁 vertex / index buffers 及其内存
        if (indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device->getDevice(), indexBuffer, nullptr);
            indexBuffer = VK_NULL_HANDLE;
        }
        if (indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->getDevice(), indexBufferMemory, nullptr);
            indexBufferMemory = VK_NULL_HANDLE;
        }
        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device->getDevice(), vertexBuffer, nullptr);
            vertexBuffer = VK_NULL_HANDLE;
        }
        if (vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->getDevice(), vertexBufferMemory, nullptr);
            vertexBufferMemory = VK_NULL_HANDLE;
        }
    }
    // void bufferInit(Texture* txture, SwapChain* sw, RenderPass* renderp, Command* cmd, Descriptor* desc){
    //     texture = txture;
    //     swapchain = sw;
    //     renderpass = renderp;
    //     command = cmd;
    //     descrpt = desc;
    // }

    std::vector<VkBuffer>& getUniformBuffers(){
        return uniformBuffers;
    }
    std::vector<VkDeviceMemory>& getUniformBuffersMemory(){
        return uniformBuffersMemory;
    }
    std::vector<void*>& getUniformBuffersMapped(){
        return uniformBuffersMapped;
    }
    VkBuffer getVertexBuffer(){
        return vertexBuffer;
    }
    VkDeviceMemory getVertexBufferMemory(){
        return vertexBufferMemory;
    }
    VkBuffer getIndexBuffer(){
        return indexBuffer;
    }
    VkDeviceMemory getIndexBufferMemory(){
        return indexBufferMemory;
    }
    

    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createFramebuffers();

    void createBuffer(VkDeviceSize size, 
    VkBufferUsageFlags usage, 
    VkMemoryPropertyFlags properties, 
    VkBuffer& buffer, 
    VkDeviceMemory& bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    std::vector<Vertex>& getVertices(){
        return vertices;
    }
    std::vector<uint32_t>& getIndices(){
        return indices;
    }

private:
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    Descriptor* descrpt;
    Device* device;
    Texture* texture;
    SwapChain* swapchain;
    RenderPass* renderpass;
    Command* command;
};