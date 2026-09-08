#pragma once

// Refer: https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/02_lighting_models.html
// lighting models:
// flat shading: 根据法线与光照方向计算，会有明显的多边形棱角（边缘）
// gouraud shading: 每个顶点计算一次光照，之后 插值 计算面的光照，导致本应该出现在多边形面中央的高光 会消失

// Phong light model: ambint + diffuse + specular
// Blinn-phong模型：用 眼睛-光源之间的半程向量 替换phong模型中的 光的反射
// Cook-Torrance Model：使用微表面理论建模表面的粗糙程度，高光项也更准确
// Oren-Nayar Model： Lambertian diffuse model的扩展，在漫反射中加入微表面粗糙度，对粗糙表面（衣物，混凝土，沙子）更好
// PBR：侧重于基于物理规则，光线与表面如何作用，
// PBR中的关键点：能量守恒（出射光能量不能超过入射光），微表面理论，菲涅尔效应，Metallic-Roughness Workflow【材料由基础色，金属度和粗糙度定义】
// 光源类型：点光源，平行光（太阳），聚光灯，面光源，基于图像的光照
// 除了光照模型之外：全局光照，Subsurface Scattering（次表面散射：光线在半透明材质内部散射的效果），
// 环境光遮蔽：一个表面能接受多少环境光，对角落和缝隙变暗增加真实感

// 👇push constants：pass small amounts of data to shaders without the overhead of descriptor sets
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>

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
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 lightPosition[4];
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

// // Resource bindings - Connect CPU resources to GPU shader registers
// [[vk::binding(0, 0)]] ConstantBuffer<UniformBufferObject> ubo;
// [[vk::binding(1, 0)]] Texture2D baseColorMap;
// [[vk::binding(1, 0)]] SamplerState baseColorSampler;
// [[vk::binding(2, 0)]] Texture2D metallicRoughnessMap;
// [[vk::binding(2, 0)]] SamplerState metallicRoughnessSampler;
// [[vk::binding(3, 0)]] Texture2D normalMap;
// [[vk::binding(3, 0)]] SamplerState normalSampler;
// [[vk::binding(4, 0)]] Texture2D occlusionMap;
// [[vk::binding(4, 0)]] SamplerState occlusionSampler;
// [[vk::binding(5, 0)]] Texture2D emissiveMap;
// [[vk::binding(5, 0)]] SamplerState emissiveSampler;

// [[vk::push_constant]] PushConstants material;

static const float PI = 3.14159265359;

class Light{

    public:
    
    Light(const Light&) = delete;
    Light& operator=(const Light&) = delete;


    private:

};