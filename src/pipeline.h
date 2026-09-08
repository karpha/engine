#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>

class Device;
class Buffer;
class Descriptor;
class RenderPass;

class Pipeline{
public:
    Pipeline(Device* dev, Descriptor* dscrp, RenderPass* rp, bool usePBR = false);
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
    
protected:
    Device* device;
private:
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    Descriptor* descriptor;
    RenderPass* renderpass;
};

// pbr pipeline 👇
class pbrPipeline : public Pipeline{
public:
    pbrPipeline(){
        std::cout << "pbrPipeline \n";
    };
    ~pbrPipeline(){};
    VkPipelineLayout pbrPipelineLayout;
    VkPipeline pbrpipeline;

    // push constant
    struct pbrPushConstantBlock{
        glm::vec4 baseColorFactor;
        float metallicFactor;
        float roughnessFactor;
        int baseColorTextureSet;
        int physicalDescriptorTextureSet;
        int normalTextureSet;
        int occlusionTextureSet;
        int emissiveTextureSet;
        float alphaMask;
        float alphaMaskCutoff;
    };
    bool createPBRPipeline();
    void pushMaterialProperties(VkCommandBuffer commandBuffer, const Model* model, uint32_t materialIndex);
    void pbrRenderTest();
};