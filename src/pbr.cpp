#include "pbr.h"
#include "buffer.h"
#include "texture.h"
#include "renderpass.h"
#include "camera.h"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

static_assert(sizeof(PbrUniforms) == 416);
static_assert(offsetof(PbrUniforms, lightPositions) == 256);
static_assert(offsetof(PbrUniforms, parameters) == 400);
static_assert(sizeof(PbrMaterialConstants) == 64);
static_assert(offsetof(PbrMaterialConstants, flags) == 48);

namespace {
void check(VkResult r, const char* operation) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(r));
}
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
glm::vec3 unit(glm::vec3 v, glm::vec3 fallback = {0, 1, 0}) {
    float len2 = glm::dot(v, v);
    return len2 > 1e-20f && std::isfinite(len2) ? v / std::sqrt(len2) : fallback;
}
glm::vec3 perpendicular(glm::vec3 n) {
    return unit(glm::cross(std::abs(n.y) < 0.9f ? glm::vec3(0,1,0) : glm::vec3(1,0,0), n));
}
struct Material {
    PbrMaterialConstants constants;
    std::array<int, 5> textures{-1, -1, -1, -1, -1};
};
struct Primitive {
    uint32_t firstIndex = 0, indexCount = 0, material = 0;
    glm::vec3 center{};
};
struct CpuModel {
    tinygltf::Model source;
    std::vector<PbrVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Primitive> primitives;
    std::vector<Material> materials;
    glm::vec3 minimum{std::numeric_limits<float>::max()};
    glm::vec3 maximum{std::numeric_limits<float>::lowest()};
};
// Checked accessor reads: account for both offsets, stride and normalization.
struct AccessorReader {
    const tinygltf::Accessor& accessor;
    const unsigned char* data;
    size_t stride, components, componentBytes;
    AccessorReader(const tinygltf::Model& model, int index) : accessor(model.accessors.at(index)) {
        require(!accessor.sparse.isSparse, "Sparse accessors are not supported by this static PBR loader");
        const auto& view = model.bufferViews.at(accessor.bufferView);
        const auto& bytes = model.buffers.at(view.buffer).data;
        int n = tinygltf::GetNumComponentsInType(accessor.type);
        int b = tinygltf::GetComponentSizeInBytes(accessor.componentType);
        int s = accessor.ByteStride(view);
        require(n > 0 && b > 0 && s > 0, "Invalid glTF accessor format");
        components = n; componentBytes = b; stride = s;
        require(stride >= components * componentBytes, "Invalid glTF accessor stride");
        require(view.byteOffset <= bytes.size() && view.byteLength <= bytes.size() - view.byteOffset,
                "glTF bufferView exceeds buffer");
        require(accessor.byteOffset <= view.byteLength, "glTF accessor offset exceeds bufferView");
        size_t available = view.byteLength - accessor.byteOffset;
        if (accessor.count) {
            require(available >= components * componentBytes &&
                    accessor.count - 1 <= (available - components * componentBytes) / stride,
                    "glTF accessor exceeds bufferView");
        }
        data = bytes.data() + view.byteOffset + accessor.byteOffset;
    }
    template<class T> T raw(const unsigned char* p) const { T v; std::memcpy(&v, p, sizeof(v)); return v; }
    double get(size_t i, size_t c) const {
        require(i < accessor.count && c < components, "glTF accessor index out of bounds");
        const auto* p = data + i * stride + c * componentBytes;
        switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT: return raw<float>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return accessor.normalized ? raw<uint8_t>(p) / 255.0 : raw<uint8_t>(p);
        case TINYGLTF_COMPONENT_TYPE_BYTE: return accessor.normalized ? std::max(-1.0, raw<int8_t>(p) / 127.0) : raw<int8_t>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return accessor.normalized ? raw<uint16_t>(p) / 65535.0 : raw<uint16_t>(p);
        case TINYGLTF_COMPONENT_TYPE_SHORT: return accessor.normalized ? std::max(-1.0, raw<int16_t>(p) / 32767.0) : raw<int16_t>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return raw<uint32_t>(p);
        default: throw std::runtime_error("Unsupported glTF component type");
        }
    }
};
glm::mat4 nodeMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 result;
        for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) result[c][r] = float(node.matrix[c * 4 + r]);
        return result;
    }
    glm::vec3 t(0), s(1); glm::quat q(1,0,0,0);
    if (node.translation.size() == 3) t = {node.translation[0], node.translation[1], node.translation[2]};
    if (node.scale.size() == 3) s = {node.scale[0], node.scale[1], node.scale[2]};
    if (node.rotation.size() == 4) q = glm::normalize(glm::quat(float(node.rotation[3]), float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2])));
    return glm::translate(glm::mat4(1), t) * glm::mat4_cast(q) * glm::scale(glm::mat4(1), s);
}
void appendPrimitive(CpuModel& out, const tinygltf::Primitive& primitive, const glm::mat4& transform) {
    require(primitive.mode == -1 || primitive.mode == TINYGLTF_MODE_TRIANGLES, "Only triangle primitives are supported");
    require(primitive.targets.empty(), "Morph targets are not supported");
    auto position = primitive.attributes.find("POSITION");
    require(position != primitive.attributes.end(), "Missing POSITION");
    AccessorReader positions(out.source, position->second);
    require(positions.components == 3 && positions.accessor.count > 0, "Invalid POSITION");
    std::vector<PbrVertex> vertices(positions.accessor.count);
    auto readAttribute = [&](const char* name, size_t count, auto write) {
        auto it = primitive.attributes.find(name);
        if (it == primitive.attributes.end()) return false;
        AccessorReader reader(out.source, it->second);
        require(reader.components == count && reader.accessor.count == vertices.size(), "Mismatched vertex attribute");
        for (size_t i = 0; i < vertices.size(); ++i) for (size_t c = 0; c < count; ++c) write(vertices[i], c, float(reader.get(i,c)));
        return true;
    };
    for (size_t i = 0; i < vertices.size(); ++i) for (int c = 0; c < 3; ++c) vertices[i].position[c] = float(positions.get(i,c));
    bool normals = readAttribute("NORMAL", 3, [](PbrVertex& v, size_t c, float f) { v.normal[c] = f; });
    bool uvs = readAttribute("TEXCOORD_0", 2, [](PbrVertex& v, size_t c, float f) { v.uv[c] = f; });
    bool tangents = readAttribute("TANGENT", 4, [](PbrVertex& v, size_t c, float f) { v.tangent[c] = f; });
    uint32_t material = primitive.material < 0 ? uint32_t(out.materials.size() - 1) : uint32_t(primitive.material);
    require(primitive.material < 0 || size_t(primitive.material) < out.source.materials.size(), "Invalid material index");
    if (!uvs) for (int texture : out.materials[material].textures) require(texture < 0, "Textured primitive has no TEXCOORD_0");
    std::vector<uint32_t> indices;
    if (primitive.indices >= 0) {
        AccessorReader reader(out.source, primitive.indices);
        require(reader.components == 1 && !reader.accessor.normalized &&
            (reader.accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
             reader.accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ||
             reader.accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT), "Invalid indices accessor");
        for (size_t i = 0; i < reader.accessor.count; ++i) {
            auto index = uint32_t(reader.get(i,0));
            require(index < vertices.size(), "Index exceeds vertex count"); indices.push_back(index);
        }
    } else for (size_t i = 0; i < vertices.size(); ++i) indices.push_back(uint32_t(i));
    require(!indices.empty() && indices.size() % 3 == 0, "Invalid triangle index count");
    if (!normals) {
        for (size_t i = 0; i < indices.size(); i += 3) {
            auto a = indices[i], b = indices[i+1], c = indices[i+2];
            glm::vec3 n = glm::cross(vertices[b].position - vertices[a].position, vertices[c].position - vertices[a].position);
            vertices[a].normal += n; vertices[b].normal += n; vertices[c].normal += n;
        }
    }
    for (auto& vertex : vertices) vertex.normal = unit(vertex.normal);
    if (!tangents) {
        // Split mirrored UV orientation when accumulating tangents. No index can
        // average opposite handedness, which would cancel the tangent basis.
        std::vector<PbrVertex> split;
        std::vector<glm::vec3> ts, bs;
        std::map<std::pair<uint32_t, bool>, uint32_t> remap;
        for (size_t i = 0; i < indices.size(); i += 3) {
            const auto& a = vertices[indices[i]]; const auto& b = vertices[indices[i+1]]; const auto& c = vertices[indices[i+2]];
            auto e1 = b.position-a.position, e2 = c.position-a.position;
            auto uv1 = b.uv-a.uv, uv2 = c.uv-a.uv;
            float det = uv1.x*uv2.y - uv1.y*uv2.x;
            glm::vec3 t(0), bt(0);
            if (std::abs(det) > 1e-12f) { t = (e1*uv2.y-e2*uv1.y)/det; bt = (e2*uv1.x-e1*uv2.x)/det; }
            for (int j = 0; j < 3; ++j) {
                auto key = std::make_pair(indices[i+j], det < 0);
                auto [it, inserted] = remap.emplace(key, uint32_t(split.size()));
                if (inserted) { split.push_back(vertices[key.first]); ts.emplace_back(0); bs.emplace_back(0); }
                indices[i+j] = it->second; ts[it->second] += t; bs[it->second] += bt;
            }
        }
        vertices = std::move(split);
        for (size_t i = 0; i < vertices.size(); ++i) {
            auto n = vertices[i].normal;
            auto t = unit(ts[i] - n*glm::dot(n,ts[i]), perpendicular(n));
            vertices[i].tangent = glm::vec4(t, glm::dot(glm::cross(n,t),bs[i]) < 0 ? -1.0f : 1.0f);
        }
    }
    float determinant = glm::determinant(glm::mat3(transform));
    require(std::isfinite(determinant) && std::abs(determinant) > 1e-12f, "Singular node transform");
    auto normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    glm::vec3 minimum(std::numeric_limits<float>::max()), maximum(std::numeric_limits<float>::lowest());
    for (auto& vertex : vertices) {
        vertex.position = glm::vec3(transform * glm::vec4(vertex.position,1));
        vertex.normal = unit(normalMatrix * vertex.normal);
        auto t = glm::mat3(transform) * glm::vec3(vertex.tangent);
        vertex.tangent = glm::vec4(unit(t-vertex.normal*glm::dot(vertex.normal,t), perpendicular(vertex.normal)), vertex.tangent.w * (determinant < 0 ? -1.0f : 1.0f));
        for (int c=0;c<3;++c) require(std::isfinite(vertex.position[c]), "Non-finite vertex position");
        minimum = glm::min(minimum,vertex.position); maximum = glm::max(maximum,vertex.position);
    }
    if (determinant < 0) for (size_t i=0;i<indices.size();i+=3) std::swap(indices[i+1],indices[i+2]);
    require(out.vertices.size()+vertices.size() <= UINT32_MAX && out.indices.size()+indices.size() <= UINT32_MAX, "Model exceeds uint32 geometry limits");
    Primitive draw{uint32_t(out.indices.size()), uint32_t(indices.size()), material, (minimum+maximum)*0.5f};
    auto base = uint32_t(out.vertices.size());
    for (auto index : indices) out.indices.push_back(base+index);
    out.vertices.insert(out.vertices.end(),vertices.begin(),vertices.end()); out.primitives.push_back(draw);
    out.minimum = glm::min(out.minimum,minimum); out.maximum = glm::max(out.maximum,maximum);
}
CpuModel readModel(const std::string& path) {
    CpuModel out; tinygltf::TinyGLTF loader; std::string error, warning;
    bool success = std::filesystem::path(path).extension() == ".glb"
        ? loader.LoadBinaryFromFile(&out.source,&error,&warning,path)
        : loader.LoadASCIIFromFile(&out.source,&error,&warning,path);
    if (!warning.empty()) std::cerr << "glTF: " << warning << '\n';
    if (!success) throw std::runtime_error("Cannot load " + path + ": " + error);
    require(out.source.extensionsRequired.empty(), "Required glTF extensions are not supported");
    for (const auto& source : out.source.materials) {
        Material m; const auto& p = source.pbrMetallicRoughness;
        for (int i=0;i<4;++i) m.constants.baseColorFactor[i] = float(p.baseColorFactor.at(i));
        for (int i=0;i<3;++i) m.constants.emissiveNormal[i] = float(source.emissiveFactor.at(i));
        m.constants.emissiveNormal.w = float(source.normalTexture.scale);
        m.constants.surface = {float(p.metallicFactor),float(p.roughnessFactor),float(source.occlusionTexture.strength),float(source.alphaCutoff)};
        m.constants.flags = {source.normalTexture.index >= 0, source.alphaMode == "MASK" ? 1 : source.alphaMode == "BLEND" ? 2 : 0, source.doubleSided, 0};
        auto textureIndex = [&](const auto& info) {
            require(info.index < 0 || (info.texCoord == 0 && info.extensions.empty()), "Only untransformed TEXCOORD_0 textures are supported");
            require(info.index < 0 || size_t(info.index) < out.source.textures.size(), "Invalid texture index");
            return info.index;
        };
        m.textures = {textureIndex(p.baseColorTexture),textureIndex(p.metallicRoughnessTexture),textureIndex(source.normalTexture),textureIndex(source.occlusionTexture),textureIndex(source.emissiveTexture)};
        if (!source.extensions.empty()) std::cerr << "PBR: optional material extensions ignored for " << source.name << " (core metallic/roughness fallback)\n";
        out.materials.push_back(m);
    }
    out.materials.emplace_back(); // glTF default material for primitive.material == -1
    std::vector<bool> visiting(out.source.nodes.size(),false);
    std::function<void(int,const glm::mat4&)> visit = [&](int index,const glm::mat4& parent) {
        const auto& node = out.source.nodes.at(index);
        require(!visiting.at(index), "Cycle in glTF node graph"); visiting[index] = true;
        require(node.skin < 0, "Skinned meshes are not supported");
        auto transform = parent * nodeMatrix(node);
        if (node.mesh >= 0) for (const auto& p : out.source.meshes.at(node.mesh).primitives) appendPrimitive(out,p,transform);
        for (auto child : node.children) visit(child,transform);
        visiting[index] = false;
    };
    require(!out.source.scenes.empty(), "glTF contains no scene");
    const auto& scene = out.source.scenes.at(out.source.defaultScene < 0 ? 0 : out.source.defaultScene);
    for (auto node : scene.nodes) visit(node,glm::mat4(1));
    require(!out.vertices.empty() && !out.indices.empty(), "glTF scene has no triangles");
    return out;
}
} // namespace

struct PbrRenderer::Impl {
    struct GpuBuffer {
        VkDevice device;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        explicit GpuBuffer(VkDevice d) : device(d) {}
        ~GpuBuffer() {
            if (mapped) vkUnmapMemory(device,memory);
            vkDestroyBuffer(device,buffer,nullptr); vkFreeMemory(device,memory,nullptr);
        }
        GpuBuffer(const GpuBuffer&) = delete;
    };
    struct GpuTexture {
        VkDevice device;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        explicit GpuTexture(VkDevice d) : device(d) {}
        ~GpuTexture() {
            vkDestroySampler(device,sampler,nullptr); vkDestroyImageView(device,view,nullptr);
            vkDestroyImage(device,image,nullptr); vkFreeMemory(device,memory,nullptr);
        }
        GpuTexture(const GpuTexture&) = delete;
    };
    struct GpuModel {
        VkDevice device;
        std::unique_ptr<GpuBuffer> vertices, indices;
        std::vector<std::unique_ptr<GpuBuffer>> uniforms;
        std::vector<std::unique_ptr<GpuTexture>> textures;
        std::vector<Material> materials;
        std::vector<Primitive> primitives;
        std::vector<VkDescriptorSet> sets;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        glm::mat4 transform{1};
        glm::vec3 minimum{}, maximum{};
        explicit GpuModel(VkDevice d) : device(d) {}
        ~GpuModel() { vkDestroyDescriptorPool(device,pool,nullptr); }
    };
    Device& device;
    Buffer& buffer;
    Texture& texture;
    RenderPass& renderPass;
    uint32_t frames;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    // alpha blending x double sided x mirrored model transform
    std::array<VkPipeline,8> pipelines{};
    std::vector<std::unique_ptr<GpuModel>> models;
    PbrUniforms frameData;
    glm::mat4 view{1};

    Impl(Device& d,Buffer& b,Texture& t,RenderPass& r,uint32_t f) : device(d),buffer(b),texture(t),renderPass(r),frames(f) {
        require(frames > 0, "PBR framesInFlight must be positive");
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device.getPhysicalDevice(), &properties);
        require(properties.apiVersion >= VK_API_VERSION_1_1, "PBR requires Vulkan 1.1 (SPIR-V 1.3)");
        frameData.lightPositions[0] = {0,5,5,1}; frameData.lightColors[0] = {90,90,90,1};
        frameData.lightPositions[1] = {-5,1,0,1}; frameData.lightColors[1] = {12,18,40,1};
        frameData.lightPositions[2] = {5,1,0,1}; frameData.lightColors[2] = {40,12,12,1};
        frameData.lightPositions[3] = {0,3,-5,1}; frameData.lightColors[3] = {15,25,15,1};
    }
    ~Impl() {
        vkDeviceWaitIdle(device.getDevice());
        models.clear();
        for (auto pipeline : pipelines) vkDestroyPipeline(device.getDevice(),pipeline,nullptr);
        vkDestroyPipelineLayout(device.getDevice(),pipelineLayout,nullptr);
        vkDestroyDescriptorSetLayout(device.getDevice(),descriptorLayout,nullptr);
    }
    std::unique_ptr<GpuBuffer> createBuffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags flags) {
        auto result = std::make_unique<GpuBuffer>(device.getDevice());
        buffer.createBuffer(size,usage,flags,result->buffer,result->memory);
        return result;
    }
    std::unique_ptr<GpuBuffer> upload(const void* data,VkDeviceSize bytes,VkBufferUsageFlags usage) {
        auto staging = createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkMapMemory(device.getDevice(),staging->memory,0,bytes,0,&staging->mapped),"map staging buffer");
        std::memcpy(staging->mapped,data,size_t(bytes));
        auto result = createBuffer(bytes,usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        buffer.copyBuffer(staging->buffer,result->buffer,bytes);
        return result;
    }
    std::unique_ptr<GpuTexture> uploadTexture(const tinygltf::Model& source,int index,bool srgb,bool normalDefault) {
        int width = 1,height = 1;
        std::vector<unsigned char> rgba = normalDefault ? std::vector<unsigned char>{128,128,255,255} : std::vector<unsigned char>{255,255,255,255};
        const tinygltf::Sampler* gltfSampler = nullptr;
        if (index >= 0) {
            const auto& tex = source.textures.at(index);
            const auto& image = source.images.at(tex.source);
            require(image.width > 0 && image.height > 0 && image.bits == 8 && image.component >= 1 && image.component <= 4,
                    "PBR textures must be decoded 8-bit images");
            width = image.width; height = image.height;
            size_t pixels = size_t(width)*size_t(height);
            require(pixels <= SIZE_MAX/4 && image.image.size() >= pixels*size_t(image.component), "Invalid decoded glTF image size");
            rgba.resize(pixels*4);
            for (size_t p=0;p<pixels;++p) {
                auto* src = image.image.data()+p*image.component;
                rgba[p*4] = src[0]; rgba[p*4+1] = image.component <= 2 ? src[0] : src[1];
                rgba[p*4+2] = image.component <= 2 ? src[0] : src[2];
                rgba[p*4+3] = image.component == 2 ? src[1] : image.component == 4 ? src[3] : 255;
            }
            if (tex.sampler >= 0) gltfSampler = &source.samplers.at(tex.sampler);
        }
        auto format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        VkFormatProperties properties{}; vkGetPhysicalDeviceFormatProperties(device.getPhysicalDevice(),format,&properties);
        VkFormatFeatureFlags blit = VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        uint32_t levels = (properties.optimalTilingFeatures & blit) == blit ? uint32_t(std::floor(std::log2(std::max(width,height))))+1 : 1;
        auto staging = createBuffer(rgba.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkMapMemory(device.getDevice(),staging->memory,0,rgba.size(),0,&staging->mapped),"map texture staging");
        std::memcpy(staging->mapped,rgba.data(),rgba.size());
        auto result = std::make_unique<GpuTexture>(device.getDevice());
        texture.createImage(width,height,levels,VK_SAMPLE_COUNT_1_BIT,format,VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,result->image,result->memory);
        texture.transitionImageLayout(result->image,format,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,levels);
        texture.copyBufferToImage(staging->buffer,result->image,width,height);
        if (levels > 1) texture.generateMipmaps(result->image,format,width,height,levels);
        else texture.transitionImageLayout(result->image,format,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1);
        result->view = texture.createImageView(result->image,format,VK_IMAGE_ASPECT_COLOR_BIT,levels);
        auto address = [](int wrap) {
            return wrap == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE :
                   wrap == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        };
        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        int minFilter = gltfSampler ? gltfSampler->minFilter : -1;
        info.magFilter = gltfSampler && gltfSampler->magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        info.minFilter = minFilter == 9728 || minFilter == 9984 || minFilter == 9986 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        info.mipmapMode = minFilter == 9984 || minFilter == 9985 ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = gltfSampler ? address(gltfSampler->wrapS) : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = gltfSampler ? address(gltfSampler->wrapT) : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.maxLod = minFilter == 9728 || minFilter == 9729 ? 0.0f : float(levels-1);
        check(vkCreateSampler(device.getDevice(),&info,nullptr,&result->sampler),"create PBR sampler");
        return result;
    }
    void createPipelines() {
        require(descriptorLayout == VK_NULL_HANDLE,"PBR initialize called twice");
        std::array<VkDescriptorSetLayoutBinding,6> bindings{};      // 对应shader 中的binding数据？
        for (uint32_t i=0;i<bindings.size();++i) {
            bindings[i].binding = i; bindings[i].descriptorCount = 1;
            bindings[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].stageFlags = i == 0 ? VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptorInfo.bindingCount = uint32_t(bindings.size()); descriptorInfo.pBindings = bindings.data();
        check(vkCreateDescriptorSetLayout(device.getDevice(),&descriptorInfo,nullptr,&descriptorLayout),"create PBR descriptor layout");

        VkPushConstantRange push{
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PbrMaterialConstants)};

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1; layoutInfo.pSetLayouts = &descriptorLayout;
        layoutInfo.pushConstantRangeCount = 1; layoutInfo.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(device.getDevice(),&layoutInfo,nullptr,&pipelineLayout),"create PBR pipeline layout");
        struct Module {
            VkDevice device; VkShaderModule handle = VK_NULL_HANDLE;
            ~Module() { vkDestroyShaderModule(device,handle,nullptr); }
        } vert{device.getDevice()},frag{device.getDevice()};
        auto loadShader = [&](const char* filename,Module& module) {
            std::ifstream file(std::filesystem::path(PBR_SHADER_DIR)/filename,std::ios::binary|std::ios::ate);
            require(bool(file),"Cannot open compiled PBR shader; build the PbrShaders target");
            auto size = file.tellg(); require(size > 0 && size % 4 == 0,"Invalid SPIR-V size");
            std::vector<uint32_t> code(size_t(size)/4); file.seekg(0); file.read(reinterpret_cast<char*>(code.data()),size);
            require(bool(file),"Cannot read SPIR-V");
            VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; info.codeSize = size_t(size); info.pCode = code.data();
            check(vkCreateShaderModule(device.getDevice(),&info,nullptr,&module.handle),"create PBR shader module");
        };
        loadShader("pbr.vert.spv",vert); loadShader("pbr.frag.spv",frag);
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vert.handle; stages[0].pName = "VSMain";       // 将这两个函数配置为两个着色器阶段
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = frag.handle; stages[1].pName = "PSMain";

        VkVertexInputBindingDescription binding{0,sizeof(PbrVertex),VK_VERTEX_INPUT_RATE_VERTEX};   // 顶点vertex 的binding是0

        std::array<VkVertexInputAttributeDescription,4> attributes{{        // 对应slang shader文件
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(PbrVertex,position)},

            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(PbrVertex,normal)},    //  binding0，location1

            {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(PbrVertex,uv)},
            {3,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(PbrVertex,tangent)}}};
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = 1; 
        input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = uint32_t(attributes.size()); input.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode = VK_POLYGON_MODE_FILL; raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; multisample.rasterizationSamples = device.getMsaaSamples();
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO}; depth.depthTestEnable = VK_TRUE; depth.depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blend.attachmentCount = 1; blend.pAttachments = &attachment;
        std::array<VkDynamicState,2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dynamicStates.data();
        
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2; 
        info.pStages = stages.data(); 
        info.pVertexInputState = &input; 
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport; 
        info.pRasterizationState = &raster; 
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depth; 
        info.pColorBlendState = &blend; 
        info.pDynamicState = &dynamic;
        info.layout = pipelineLayout; 
        info.renderPass = renderPass.getRenderpass();
        for (uint32_t key=0;key<pipelines.size();++key) {   // 对于不同的绘制情况，先批量设置基础attribute，之后再根据不同情况，分别设置对应的剩余attri
            bool transparent = key & 1, doubleSided = key & 2, mirrored = key & 4;
            attachment.blendEnable = transparent; 
            depth.depthWriteEnable = !transparent;
            raster.cullMode = doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
            raster.frontFace = mirrored ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
            check(vkCreateGraphicsPipelines(device.getDevice(),VK_NULL_HANDLE,1,&info,nullptr,&pipelines[key]),"create PBR graphics pipeline");
        }
    }
    size_t load(const std::string& path) {
        require(pipelineLayout != VK_NULL_HANDLE,"Initialize PBR before loading models");
        auto cpu = readModel(path);
        auto model = std::make_unique<GpuModel>(device.getDevice());
        model->minimum = cpu.minimum; model->maximum = cpu.maximum;
        model->materials = cpu.materials; model->primitives = cpu.primitives;

        model->vertices = upload(
            cpu.vertices.data(),
            cpu.vertices.size()*sizeof(PbrVertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        model->indices = upload(cpu.indices.data(),cpu.indices.size()*sizeof(uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        for (uint32_t frame=0;frame<frames;++frame) {
            auto ubo = createBuffer(sizeof(PbrUniforms),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            check(vkMapMemory(device.getDevice(),ubo->memory,0,sizeof(PbrUniforms),0,&ubo->mapped),"map PBR uniforms");
            model->uniforms.push_back(std::move(ubo));
        }
        require(model->materials.size() <= UINT32_MAX / frames / 5,"Too many PBR materials");
        uint32_t count = uint32_t(model->materials.size())*frames;
        std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,count},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,count*5}}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pool.maxSets = count; pool.poolSizeCount = 2; pool.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device.getDevice(),&pool,nullptr,&model->pool),"create PBR descriptor pool");
        std::vector<VkDescriptorSetLayout> layouts(count,descriptorLayout); model->sets.resize(count);
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; allocate.descriptorPool = model->pool; allocate.descriptorSetCount = count; allocate.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(device.getDevice(),&allocate,model->sets.data()),"allocate PBR descriptor sets");
        std::map<std::pair<int,int>,size_t> cache;
        for (size_t m=0;m<model->materials.size();++m) {
            std::array<VkDescriptorImageInfo,5> images{};
            for (int slot=0;slot<5;++slot) {
                int index = model->materials[m].textures[slot]; bool srgb = slot == 0 || slot == 4;
                auto key = std::make_pair(index,index < 0 && slot == 2 ? 2 : int(srgb));
                auto it = cache.find(key);
                if (it == cache.end()) {
                    auto image = uploadTexture(cpu.source,index,srgb,slot == 2);
                    it = cache.emplace(key,model->textures.size()).first;
                    model->textures.push_back(std::move(image));
                }
                const auto& image = *model->textures[it->second]; images[slot] = {image.sampler,image.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            }
            for (uint32_t frame=0;frame<frames;++frame) {
                VkDescriptorBufferInfo ubo{model->uniforms[frame]->buffer,0,sizeof(PbrUniforms)};
                std::array<VkWriteDescriptorSet,6> writes{};
                for (uint32_t binding=0;binding<writes.size();++binding) {
                    auto& write = writes[binding]; 
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = model->sets[m*frames+frame]; 
                    write.dstBinding = binding; 
                    write.descriptorCount = 1;
                    write.descriptorType = binding == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    if (binding == 0) write.pBufferInfo = &ubo; else write.pImageInfo = &images[binding-1];
                }

                vkUpdateDescriptorSets(device.getDevice(),uint32_t(writes.size()),writes.data(),0,nullptr);     // descriptor, set 0 
                // ubo的数据格式和布局需要与shader中相同

            }
        }
        std::cout << "PBR loaded " << path << ": " << cpu.vertices.size() << " vertices, " << cpu.indices.size()/3 << " triangles, " << model->primitives.size() << " primitives\n";
        models.push_back(std::move(model)); return models.size()-1;
    }
};

PbrRenderer::PbrRenderer(Device& d,Buffer& b,Texture& t,RenderPass& r,uint32_t frames) : impl(std::make_unique<Impl>(d,b,t,r,frames)) {}
PbrRenderer::~PbrRenderer() = default;
void PbrRenderer::initialize() { impl->createPipelines(); }
size_t PbrRenderer::loadModel(const std::string& path) { return impl->load(path); }
void PbrRenderer::setModelTransform(size_t model,const glm::mat4& transform) {
    float determinant = glm::determinant(glm::mat3(transform));
    require(std::isfinite(determinant) && std::abs(determinant)>1e-12f,"Singular PBR model transform");
    impl->models.at(model)->transform = transform;
}
void PbrRenderer::fitModel(size_t index,const glm::vec3& center,float size) {
    const auto& model = *impl->models.at(index); auto extent = model.maximum-model.minimum;
    float dimension = std::max({extent.x,extent.y,extent.z});
    require(size>0 && dimension>1e-8f,"Invalid model bounds or size");
    setModelTransform(index,glm::translate(glm::mat4(1),center)*glm::scale(glm::mat4(1),glm::vec3(size/dimension))*glm::translate(glm::mat4(1),-(model.minimum+model.maximum)*0.5f));
}
void PbrRenderer::setLight(uint32_t index,glm::vec3 position,glm::vec3 intensity) {
    require(index<4,"PBR supports four point lights"); impl->frameData.lightPositions[index] = glm::vec4(position,1);
    impl->frameData.lightColors[index] = glm::vec4(glm::max(intensity,glm::vec3(0)),1);
}
void PbrRenderer::setExposure(float exposure) { require(std::isfinite(exposure) && exposure>0,"Invalid exposure"); impl->frameData.parameters.x = exposure; }
void PbrRenderer::updateFrame(uint32_t frame,const Camera& camera,VkExtent2D extent,VkFormat format) {
    require(frame<impl->frames && extent.width && extent.height,"Invalid PBR frame or extent");
    auto ubo = impl->frameData; 
    ubo.view = camera.getViewMatrix(); 
    impl->view = ubo.view;
    ubo.proj = camera.getProjectionMatrix(float(extent.width)/float(extent.height)); ubo.proj[1][1] *= -1;
    ubo.camPos = glm::vec4(camera.getPosition(),1);
    ubo.parameters.z = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_A8B8G8R8_SRGB_PACK32 ? 1.0f : 0.0f;
    for (auto& model : impl->models) {
        ubo.model = model->transform; 
        ubo.normalMatrix = glm::transpose(glm::inverse(ubo.model));

        std::memcpy(model->uniforms[frame]->mapped,&ubo,sizeof(ubo));

    }
}
void PbrRenderer::draw(VkCommandBuffer command,uint32_t frame,VkExtent2D extent) {
    require(frame<impl->frames,"Invalid PBR frame");
    VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1}; VkRect2D scissor{{0,0},extent};
    vkCmdSetViewport(command,0,1,&viewport); vkCmdSetScissor(command,0,1,&scissor);
    struct Draw { Impl::GpuModel* model; const Primitive* primitive; float depth; };
    std::vector<Draw> transparent;
    auto draw = [&](Impl::GpuModel& model,const Primitive& primitive) {

        const auto& material = model.materials[primitive.material].constants;

        uint32_t key = (material.flags.y == 2 ? 1 : 0) | (material.flags.z ? 2 : 0) | (glm::determinant(glm::mat3(model.transform))<0 ? 4 : 0);

        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,impl->pipelines[key]);       // 使用pipelines的设置，在draw阶段绘制

        VkDeviceSize offset = 0; 

        vkCmdBindVertexBuffers(command,0,1,&model.vertices->buffer,&offset);

        vkCmdBindIndexBuffer(command,model.indices->buffer,0,VK_INDEX_TYPE_UINT32);
        auto set = model.sets[primitive.material*impl->frames+frame];

        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,impl->pipelineLayout,0,1,&set,0,nullptr);

        vkCmdPushConstants(command,impl->pipelineLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(material),&material);

        vkCmdDrawIndexed(command,primitive.indexCount,1,primitive.firstIndex,0,0);
    };
    for (auto& model : impl->models) for (const auto& primitive : model->primitives) {
        if (model->materials[primitive.material].constants.flags.y == 2) {
            float depth = (impl->view*model->transform*glm::vec4(primitive.center,1)).z;
            transparent.push_back({model.get(),&primitive,depth});
        } else draw(*model,primitive);
    }
    std::stable_sort(transparent.begin(),transparent.end(),[](const Draw& a,const Draw& b) { return a.depth<b.depth; });
    for (const auto& item : transparent) draw(*item.model,*item.primitive);
}
void PbrRenderer::validateAssets(const std::string& directory) {
    // Regression: normalized interleaved data with independent view/accessor offsets.
    tinygltf::Model test;
    test.buffers.resize(1); test.buffers[0].data.resize(20,0);
    test.bufferViews.resize(1); auto& view = test.bufferViews[0];
    view.buffer = 0; view.byteOffset = 4; view.byteLength = 16; view.byteStride = 4;
    test.accessors.resize(1); auto& accessor = test.accessors[0];
    accessor.bufferView = 0; accessor.byteOffset = 2; accessor.count = 2;
    accessor.type = TINYGLTF_TYPE_VEC2; accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    accessor.normalized = true;
    test.buffers[0].data[6] = 255; test.buffers[0].data[7] = 0;
    test.buffers[0].data[10] = 128; test.buffers[0].data[11] = 255;
    AccessorReader reader(test,0);
    require(reader.get(0,0) == 1.0 && reader.get(1,1) == 1.0 && std::abs(reader.get(1,0)-128.0/255.0)<1e-8,
            "Accessor offset/stride/normalization regression");
    accessor.count = 100;
    bool rejected = false;
    try { AccessorReader invalid(test,0); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected,"Out-of-range accessor was not rejected");
    for (const auto& name : {"DamagedHelmet","FlightHelmet"}) {
        auto model = readModel((std::filesystem::path(directory)/name/(std::string(name)+".gltf")).string());
        require(model.primitives.size() == (std::string(name)=="DamagedHelmet" ? 1 : 6),"Unexpected primitive count");
        for (auto index : model.indices) require(index<model.vertices.size(),"Index outside merged mesh");
        for (const auto& v : model.vertices) {
            require(std::abs(glm::length(v.normal)-1)<1e-3f,"Non-unit generated normal");
            require(std::abs(glm::length(glm::vec3(v.tangent))-1)<1e-3f,"Non-unit generated tangent");
            require(std::abs(glm::dot(v.normal,glm::vec3(v.tangent)))<1e-3f,"Tangent not orthogonal to normal");
            require(std::abs(std::abs(v.tangent.w)-1)<1e-3f,"Invalid tangent handedness");
        }
        std::cout << "PBR CPU validation passed: " << name << " (" << model.vertices.size() << " vertices, " << model.indices.size()/3 << " triangles)\n";
    }
}
