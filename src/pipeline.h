#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>

class Device;
class Buffer;
class Descriptor;
class RenderPass;

// input from vertex shader
struct VSInput
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 UV;
    glm::vec4 Tangent;      // w component = handedness
    /* data */
};
// output from vertex shader, input to fragment shader
struct VSOutput{
    glm::vec4 Position;
    glm::vec3 WorldPos;
    glm::vec3 Normal;
    glm::vec2 UV;
    glm::vec4 Tangent;
};
// uniform buffer
// uniform buffers are larger, read-only memory blocks,store data shared across many draw calls
struct pbrUniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 lightPositions[4];
    glm::vec4 lightColors[4];
    glm::vec4 camPos;
    float exposure;     // HDR exposure control
    float gamma;       // gamma correction
    float prefilteredCubeMipLevels;     // IBL  environment map mip level
    float scaleIBLAmbient;      // IBL ambient contribution scale
};

// push constants
// push constants are smaller(limited to 128 bytes or less),
// but much faster for frequently changing per-object data like material properties,
struct PushConstants{
    glm::vec4 baseColorFactor;
    float metallicFactor;
    // 先把代码抄下来，再理解
    float roughnessFactor;
    int baseColorTextureSet;    // Texture binding index for base color(-1 = none)
    int physicalDescriptorTextureSet;   // texture binding for metallic/roughness
    int normalTextureSet;       // texture binding for normal map
    int occlusionTextureSet;        //texture binding for ambient occlusion
    int emissiveTextureSet;     //texture binding for emissive maps
    float alphaMask;        // alpha making flag
    float alphaMaskCutoff;      // alpha cutoff threshold
};

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