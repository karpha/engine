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