#pragma once
#include <vulkan/vulkan.h>
#include <span>
#include <limits>
#include <glm/glm.hpp>
#include <memory>
#include <string>

class Device;
class Buffer;
class Texture;
class RenderPass;
class Camera;

// Resource identity and per-object render data shared with Engine.
struct ModelHandle {
    size_t value = std::numeric_limits<size_t>::max();
    bool operator==(const ModelHandle&) const = default;
};
struct ModelBounds { glm::vec3 minimum{}, maximum{}; };
struct RenderItem { ModelHandle model; glm::mat4 worldTransform{1}; };

// Matches pbrCommon.slang, compiled with column-major matrices.
struct PbrVertex {
    glm::vec3 position{}, normal{};
    glm::vec2 uv{};
    glm::vec4 tangent{};
};
struct alignas(16) PbrUniforms {
    glm::mat4 model{1}, view{1}, proj{1}, normalMatrix{1};
    glm::vec4 lightPositions[4]{}, lightColors[4]{};
    glm::vec4 camPos{};
    glm::vec4 parameters{1, 2.2f, 1, 0.03f};
};
struct alignas(16) PbrMaterialConstants {
    glm::vec4 baseColorFactor{1};
    glm::vec4 emissiveNormal{0, 0, 0, 1};
    glm::vec4 surface{1, 1, 1, 0.5f};
    glm::ivec4 flags{0};
};

// Owns PBR models, textures, descriptors, buffers and pipelines.
// Helpers must outlive this object. Static glTF meshes; no skinning or IBL.
class PbrRenderer {
public:
    PbrRenderer(Device&, Buffer&, Texture&, RenderPass&, uint32_t framesInFlight);
    ~PbrRenderer();
    PbrRenderer(const PbrRenderer&) = delete;
    PbrRenderer& operator=(const PbrRenderer&) = delete;
    void initialize();
    size_t loadModel(const std::string& path);
    ModelBounds modelBounds(size_t model) const;
    // Update only after waiting for the current frame's fence.
    void updateFrame(uint32_t frame, std::span<const RenderItem>, const Camera&, VkExtent2D, VkFormat);
    // Caller owns begin/end render pass and command buffer.
    void draw(VkCommandBuffer, uint32_t frame, VkExtent2D);
    void setLight(uint32_t index, glm::vec3 position, glm::vec3 intensity);
    void setExposure(float exposure);
    // Exercises the production CPU loader without creating a Vulkan device.
    static void validateAssets(const std::string& modelDirectory);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
