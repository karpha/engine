#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class Device;
class Buffer;
class Descriptor;
class RenderPass;

class Pipeline{
public:
    Pipeline(Device* dev, Descriptor* dscrp, RenderPass* rp);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    VkPipeline getGraphicsPipeline(){
        return graphicsPipeline;
    }
    VkPipelineLayout getPipelineLayout(){
        return pipelineLayout;
    }

    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code);

private:
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    Device* device;
    Descriptor* descriptor;
    RenderPass* renderpass;
};