#include "ImGui.h"
// Absolute package path avoids the ImGui.h/imgui.h collision on Windows.
#include ENGINE_IMGUI_CORE_HEADER
#include "device.h"
#include "renderpass.h"
#include "swapchain.h"
#include "camera.h"
#include "pbr.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

static_assert(IMGUI_VERSION_NUM >= 19200, "The custom renderer requires Dear ImGui 1.92+");
namespace {
// CMake extracts these sources into build/ and compiles SPIR-V.
[[maybe_unused]] constexpr char vertexShader[] = R"glsl_vertex(
#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 color;
layout(push_constant) uniform Transform { vec2 scale; vec2 translate; } transform;
layout(location=0) out vec2 outUV;
layout(location=1) out vec4 outColor;
void main() {
    outUV = uv;
    outColor = color;
    gl_Position = vec4(position * transform.scale + transform.translate, 0, 1);
}
)glsl_vertex";
[[maybe_unused]] constexpr char fragmentShader[] = R"glsl_fragment(
#version 450
layout(set=0,binding=0) uniform sampler2D fontTexture;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 color;
layout(location=0) out vec4 outColor;
void main() { outColor = color * texture(fontTexture, uv); }
)glsl_fragment";
void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string("ImGui: ") + operation + " (" + std::to_string(result) + ")");
}
VkShaderModule loadShader(VkDevice device, const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error(std::string("ImGui: cannot open shader ") + path);
    const auto size = static_cast<size_t>(file.tellg());
    if (!size || size % 4) throw std::runtime_error("ImGui: invalid SPIR-V size");
    std::vector<uint32_t> code(size / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(code.data()), size)) throw std::runtime_error("ImGui: cannot read shader");
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = size; info.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &info, nullptr, &module), "create shader");
    return module;
}
ImGuiKey mapKey(int key) {
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) return static_cast<ImGuiKey>(ImGuiKey_A + key - GLFW_KEY_A);
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) return static_cast<ImGuiKey>(ImGuiKey_0 + key - GLFW_KEY_0);
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F24) return static_cast<ImGuiKey>(ImGuiKey_F1 + key - GLFW_KEY_F1);
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + key - GLFW_KEY_KP_0);
    switch (key) {
#define KEY(glfw, imgui) case GLFW_KEY_##glfw: return ImGuiKey_##imgui
        KEY(TAB, Tab); KEY(LEFT, LeftArrow); KEY(RIGHT, RightArrow); KEY(UP, UpArrow); KEY(DOWN, DownArrow);
        KEY(PAGE_UP, PageUp); KEY(PAGE_DOWN, PageDown); KEY(HOME, Home); KEY(END, End);
        KEY(INSERT, Insert); KEY(DELETE, Delete); KEY(BACKSPACE, Backspace); KEY(SPACE, Space);
        KEY(ENTER, Enter); KEY(ESCAPE, Escape); KEY(APOSTROPHE, Apostrophe); KEY(COMMA, Comma);
        KEY(MINUS, Minus); KEY(PERIOD, Period); KEY(SLASH, Slash); KEY(SEMICOLON, Semicolon);
        KEY(EQUAL, Equal); KEY(LEFT_BRACKET, LeftBracket); KEY(BACKSLASH, Backslash);
        KEY(RIGHT_BRACKET, RightBracket); KEY(GRAVE_ACCENT, GraveAccent); KEY(CAPS_LOCK, CapsLock);
        KEY(SCROLL_LOCK, ScrollLock); KEY(NUM_LOCK, NumLock); KEY(PRINT_SCREEN, PrintScreen); KEY(PAUSE, Pause);
        KEY(KP_DECIMAL, KeypadDecimal); KEY(KP_DIVIDE, KeypadDivide); KEY(KP_MULTIPLY, KeypadMultiply);
        KEY(KP_SUBTRACT, KeypadSubtract); KEY(KP_ADD, KeypadAdd); KEY(KP_ENTER, KeypadEnter); KEY(KP_EQUAL, KeypadEqual);
        KEY(LEFT_SHIFT, LeftShift); KEY(LEFT_CONTROL, LeftCtrl); KEY(LEFT_ALT, LeftAlt); KEY(LEFT_SUPER, LeftSuper);
        KEY(RIGHT_SHIFT, RightShift); KEY(RIGHT_CONTROL, RightCtrl); KEY(RIGHT_ALT, RightAlt); KEY(RIGHT_SUPER, RightSuper);
        KEY(MENU, Menu);
#undef KEY
        default: return ImGuiKey_None;
    }
}
}
ImGuiForVulkan* ImGuiForVulkan::active = nullptr;
ImGuiForVulkan::~ImGuiForVulkan() { shutdown(); }
void ImGuiForVulkan::init(GLFWwindow* win, Instance&, Device& dev, SwapChain& sw, RenderPass& rp, uint32_t count) {
    if (context || active) throw std::runtime_error("ImGui: only one GUI instance is supported");
    if (!win || count == 0) throw std::runtime_error("ImGui: invalid initialization arguments");
    window = win; device = dev.getDevice(); physicalDevice = dev.getPhysicalDevice();
    swapchain = &sw; renderpass = &rp; samples = dev.getMsaaSamples();
    try {
        frames.resize(count);
        IMGUI_CHECKVERSION();
        context = ImGui::CreateContext();
        ImGui::SetCurrentContext(context);
        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        io.BackendRendererName = "engine_vulkan"; io.BackendPlatformName = "engine_glfw";
        io.IniFilename = nullptr;
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        auto& platform = ImGui::GetPlatformIO();
        platform.Renderer_TextureMaxWidth = static_cast<int>(properties.limits.maxImageDimension2D);
        platform.Renderer_TextureMaxHeight = static_cast<int>(properties.limits.maxImageDimension2D);
        setStyle(0); initResources(); initInput();
    } catch (...) { shutdown(); throw; }
}
uint32_t ImGuiForVulkan::memoryType(uint32_t bits, VkMemoryPropertyFlags flags) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags) return i;
    throw std::runtime_error("ImGui: no compatible memory type");
}
ImGuiForVulkan::Buffer ImGuiForVulkan::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    Buffer b;
    try {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &info, nullptr, &b.handle), "create buffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, b.handle, &requirements);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkAllocateMemory(device, &alloc, nullptr, &b.memory), "allocate buffer");
        check(vkBindBufferMemory(device, b.handle, b.memory, 0), "bind buffer");
        check(vkMapMemory(device, b.memory, 0, size, 0, &b.mapped), "map buffer");
        b.size = size;
    } catch (...) { destroyBuffer(b); throw; }
    return b;
}
void ImGuiForVulkan::destroyBuffer(Buffer& b) noexcept {
    if (b.mapped) vkUnmapMemory(device, b.memory);
    if (b.handle) vkDestroyBuffer(device, b.handle, nullptr);
    if (b.memory) vkFreeMemory(device, b.memory, nullptr);
    b = {};
}
void ImGuiForVulkan::destroyTexture(Texture& t) noexcept {
    if (t.descriptor) vkFreeDescriptorSets(device, descriptorPool, 1, &t.descriptor);
    if (t.view) vkDestroyImageView(device, t.view, nullptr);
    if (t.image) vkDestroyImage(device, t.image, nullptr);
    if (t.memory) vkFreeMemory(device, t.memory, nullptr);
    t = {};
}
void ImGuiForVulkan::initResources() {
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR; 
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    check(vkCreateSampler(device, &si, nullptr, &sampler), "create sampler");

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; 
    pi.maxSets = 1024;
    pi.poolSizeCount = 1; 
    pi.pPoolSizes = &poolSize;
    check(vkCreateDescriptorPool(device, &pi, nullptr, &descriptorPool), "create descriptor pool");
    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.bindingCount = 1; 
    li.pBindings = &binding;
    check(vkCreateDescriptorSetLayout(device, &li, nullptr, &descriptorSetLayout), "create descriptor layout");
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock)};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount = 1; 
    layout.pSetLayouts = &descriptorSetLayout;
    layout.pushConstantRangeCount = 1; 
    layout.pPushConstantRanges = &push;
    check(vkCreatePipelineLayout(device, &layout, nullptr, &pipelineLayout), "create pipeline layout");
    VkPipelineCacheCreateInfo cache{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    check(vkCreatePipelineCache(device, &cache, nullptr, &pipelineCache), "create pipeline cache");
    createPipeline();
}
void ImGuiForVulkan::createPipeline() {
    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    try {
        vert = loadShader(device, IMGUI_SHADER_DIR "/imgui.vert.spv");
        frag = loadShader(device, IMGUI_SHADER_DIR "/imgui.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& stage : stages) { stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName = "main"; }
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vert;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = frag;
        VkVertexInputBindingDescription binding{0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[] = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)},
            {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)},
            {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)}
        };
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &binding;
        vi.vertexAttributeDescriptionCount = 3; vi.pVertexAttributeDescriptions = attributes;
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount = vp.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = samples;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        VkPipelineColorBlendAttachmentState blend{};
        blend.blendEnable = VK_TRUE; blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1; cb.pAttachments = &blend;
        VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2; info.pStages = stages; info.pVertexInputState = &vi; info.pInputAssemblyState = &ia;
        info.pViewportState = &vp; info.pRasterizationState = &rs; info.pMultisampleState = &ms;
        info.pDepthStencilState = &ds; info.pColorBlendState = &cb; info.pDynamicState = &dynamic;
        info.layout = pipelineLayout; info.renderPass = renderpass->getRenderpass();
        VkPipeline replacement = VK_NULL_HANDLE;
        const auto result = vkCreateGraphicsPipelines(device, pipelineCache, 1, &info, nullptr, &replacement);
        if (result != VK_SUCCESS && replacement) vkDestroyPipeline(device, replacement, nullptr);
        check(result, "create graphics pipeline");
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = replacement;
    } catch (...) {
        if (vert) vkDestroyShaderModule(device, vert, nullptr);
        if (frag) vkDestroyShaderModule(device, frag, nullptr);
        throw;
    }
    vkDestroyShaderModule(device, vert, nullptr); vkDestroyShaderModule(device, frag, nullptr);
}

void ImGuiForVulkan::updateTexture(ImTextureData* tex, VkCommandBuffer cmd, Frame& frame) {
    if (tex->Status == ImTextureStatus_WantDestroy) {
        auto it = textures.find(tex);
        if (it != textures.end()) {
            // Older submissions on this graphics queue finish before this frame's fence.
            frame.retired.push_back(it->second);
            textures.erase(it);
        }
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
        return;
    }
    if (tex->Status != ImTextureStatus_WantCreate && tex->Status != ImTextureStatus_WantUpdates) return;
    // Copy-on-write avoids modifying descriptors/images sampled by earlier frames.
    // Upload the complete CPU atlas for both initial and incremental requests.
    Texture replacement;
    try {
        const bool alpha = tex->Format == ImTextureFormat_Alpha8;
        VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = alpha ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
        ii.extent = {static_cast<uint32_t>(tex->Width), static_cast<uint32_t>(tex->Height), 1};
        ii.mipLevels = ii.arrayLayers = 1; ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL; ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateImage(device, &ii, nullptr, &replacement.image), "create texture");
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device, replacement.image, &requirements);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(device, &alloc, nullptr, &replacement.memory), "allocate texture");
        check(vkBindImageMemory(device, replacement.image, replacement.memory, 0), "bind texture");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = replacement.image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = ii.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (alpha) view.components = {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_R};
        check(vkCreateImageView(device, &view, nullptr, &replacement.view), "create texture view");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        da.descriptorPool = descriptorPool; da.descriptorSetCount = 1; da.pSetLayouts = &descriptorSetLayout;
        check(vkAllocateDescriptorSets(device, &da, &replacement.descriptor), "allocate texture descriptor");
        VkDescriptorImageInfo di{sampler, replacement.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = replacement.descriptor; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &di;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        frame.staging.emplace_back();
        auto& upload = frame.staging.back();
        upload = createBuffer(static_cast<VkDeviceSize>(tex->Width) * tex->Height * tex->BytesPerPixel, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memcpy(upload.mapped, tex->GetPixels(), static_cast<size_t>(upload.size));
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = replacement.image; barrier.subresourceRange = view.subresourceRange;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = ii.extent;
        vkCmdCopyBufferToImage(cmd, upload.handle, replacement.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        auto [it, inserted] = textures.try_emplace(tex);
        if (!inserted) frame.retired.push_back(it->second);
        it->second = replacement;
    } catch (...) { destroyTexture(replacement); throw; }
    tex->SetTexID(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(replacement.descriptor)));
    tex->SetStatus(ImTextureStatus_OK);
}
void ImGuiForVulkan::updateBuffers(Frame& frame) {
    auto* data = ImGui::GetDrawData();
    if (!data || data->TotalVtxCount == 0 || data->TotalIdxCount == 0) return;
    auto grow = [this](Buffer& b, VkDeviceSize size, VkBufferUsageFlags usage) {
        if (b.size >= size) return;
        Buffer replacement = createBuffer(size + size / 2 + 4096, usage);
        destroyBuffer(b); b = replacement;
    };
    grow(frame.vertices, static_cast<VkDeviceSize>(data->TotalVtxCount) * sizeof(ImDrawVert), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    grow(frame.indices, static_cast<VkDeviceSize>(data->TotalIdxCount) * sizeof(ImDrawIdx), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    auto* vertices = static_cast<ImDrawVert*>(frame.vertices.mapped);
    auto* indices = static_cast<ImDrawIdx*>(frame.indices.mapped);
    for (const auto* list : data->CmdLists) {
        std::memcpy(vertices, list->VtxBuffer.Data, list->VtxBuffer.Size * sizeof(ImDrawVert));
        std::memcpy(indices, list->IdxBuffer.Data, list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vertices += list->VtxBuffer.Size; 
        indices += list->IdxBuffer.Size;
    }
}
void ImGuiForVulkan::prepareFrame(VkCommandBuffer cmd, uint32_t index) {
    ImGui::SetCurrentContext(context);
    currentFrame = index;
    auto& frame = frames.at(index);
    // Caller has waited for this frame slot's fence.
    for (auto& b : frame.staging) destroyBuffer(b);
    for (auto& t : frame.retired) destroyTexture(t);
    frame.staging.clear(); frame.retired.clear();
    auto* data = ImGui::GetDrawData();
    if (data && data->Textures)
        for (auto* tex : *data->Textures) updateTexture(tex, cmd, frame);
    updateBuffers(frame);
}
void ImGuiForVulkan::bindRenderState(VkCommandBuffer cmd) {
    auto* data = ImGui::GetDrawData();
    auto& frame = frames[currentFrame];
    const auto extent = swapchain->getSwapchainExtent();
    VkViewport viewport{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &frame.vertices.handle, &offset);
    vkCmdBindIndexBuffer(cmd, frame.indices.handle, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    PushConstBlock push{{2 / data->DisplaySize.x, 2 / data->DisplaySize.y}, {0, 0}};
    push.translate[0] = -1 - data->DisplayPos.x * push.scale[0];
    push.translate[1] = -1 - data->DisplayPos.y * push.scale[1];
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
}
void ImGuiForVulkan::drawFrame(VkCommandBuffer cmd) {
    ImGui::SetCurrentContext(context);
    auto* data = ImGui::GetDrawData();
    if (!data || data->TotalVtxCount == 0 || data->TotalIdxCount == 0 || data->DisplaySize.x <= 0 || data->DisplaySize.y <= 0) return;
    bindRenderState(cmd);
    const auto extent = swapchain->getSwapchainExtent();
    const float sx = extent.width / data->DisplaySize.x, sy = extent.height / data->DisplaySize.y;
    uint32_t indexOffset = 0;
    int32_t vertexOffset = 0;
    for (const auto* list : data->CmdLists) {
        for (const auto& draw : list->CmdBuffer) {
            if (draw.UserCallback) {
                if (draw.UserCallback == ImDrawCallback_ResetRenderState) bindRenderState(cmd);
                else draw.UserCallback(list, &draw);
                continue;
            }
            const float x1 = std::clamp((draw.ClipRect.x - data->DisplayPos.x) * sx, 0.0f, static_cast<float>(extent.width));
            const float y1 = std::clamp((draw.ClipRect.y - data->DisplayPos.y) * sy, 0.0f, static_cast<float>(extent.height));
            const float x2 = std::clamp((draw.ClipRect.z - data->DisplayPos.x) * sx, 0.0f, static_cast<float>(extent.width));
            const float y2 = std::clamp((draw.ClipRect.w - data->DisplayPos.y) * sy, 0.0f, static_cast<float>(extent.height));
            if (x2 <= x1 || y2 <= y1) continue;
            VkRect2D clip{{static_cast<int32_t>(x1), static_cast<int32_t>(y1)},
                {static_cast<uint32_t>(std::ceil(x2)) - static_cast<uint32_t>(x1), static_cast<uint32_t>(std::ceil(y2)) - static_cast<uint32_t>(y1)}};
            vkCmdSetScissor(cmd, 0, 1, &clip);
            auto descriptor = reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(draw.GetTexID()));
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptor, 0, nullptr);
            vkCmdDrawIndexed(cmd, draw.ElemCount, 1, indexOffset + draw.IdxOffset, vertexOffset + static_cast<int32_t>(draw.VtxOffset), 0);
        }
        indexOffset += list->IdxBuffer.Size; vertexOffset += list->VtxBuffer.Size;
    }
    VkRect2D full{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &full);
}
void ImGuiForVulkan::onSwapChainRecreated() { if (context) createPipeline(); }
void ImGuiForVulkan::setStyle(uint32_t index) {
    ImGui::SetCurrentContext(context);
    if (index == 1) ImGui::StyleColorsClassic();
    else if (index == 3) ImGui::StyleColorsLight();
    else ImGui::StyleColorsDark();
    if (index == 0) {
        auto& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.5f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.8f, 0.05f, 0.05f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    }
}
void ImGuiForVulkan::newFrame(const Camera& camera, PbrRenderer* pbr) {
    ImGui::SetCurrentContext(context);
    updateInput();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Vulkan Renderer")) {
        const auto& io = ImGui::GetIO();
        ImGui::Text("%.1f FPS / %.2f ms", io.Framerate, io.Framerate > 0 ? 1000 / io.Framerate : 0);
        auto position = camera.getPosition();
        ImGui::Text("Camera: %.2f, %.2f, %.2f", position.x, position.y, position.z);
        ImGui::TextUnformatted("Esc: release/capture mouse");
        ImGui::TextUnformatted("WASD / Space / Ctrl: move camera");
        if (ImGui::Combo("Style", &styleIndex, "Vulkan\0Classic\0Dark\0Light\0")) setStyle(styleIndex);
        if (pbr && ImGui::SliderFloat("Exposure", &exposure, 0.1f, 5.0f)) pbr->setExposure(exposure);
        ImGui::Checkbox("Dear ImGui demo", &showDemo);
    }
    ImGui::End();
    if (showDemo) ImGui::ShowDemoWindow(&showDemo);
    ImGui::Render();
}

bool ImGuiForVulkan::getWantKeyCapture() const {
    if (!context) return false;
    ImGui::SetCurrentContext(context);
    return ImGui::GetIO().WantCaptureKeyboard;
}
bool ImGuiForVulkan::getWantMouseCapture() const {
    if (!context) return false;
    ImGui::SetCurrentContext(context);
    return ImGui::GetIO().WantCaptureMouse;
}
void ImGuiForVulkan::handleKey(int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_RELEASE) return;
    ImGui::SetCurrentContext(context);
    auto& io = ImGui::GetIO();
    // Camera mode owns input; releases must still reach ImGui to avoid stuck keys.
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED && action != GLFW_RELEASE) return;
    io.AddKeyEvent(ImGuiMod_Ctrl, (mods & GLFW_MOD_CONTROL) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (mods & GLFW_MOD_SHIFT) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (mods & GLFW_MOD_ALT) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (mods & GLFW_MOD_SUPER) != 0);
    const auto mapped = mapKey(key);
    if (mapped != ImGuiKey_None) {
        io.AddKeyEvent(mapped, action == GLFW_PRESS);
        io.SetKeyEventNativeData(mapped, key, scancode);
    }
}
void ImGuiForVulkan::handleMousePos(float x, float y) {
    ImGui::SetCurrentContext(context);
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) ImGui::GetIO().AddMousePosEvent(x, y);
}
void ImGuiForVulkan::handleMouseButton(int button, bool pressed) {
    ImGui::SetCurrentContext(context);
    if (button >= 0 && button < 5 && (!pressed || glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED))
        ImGui::GetIO().AddMouseButtonEvent(button, pressed);
}
void ImGuiForVulkan::charPressed(uint32_t codepoint) {
    ImGui::SetCurrentContext(context);
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) ImGui::GetIO().AddInputCharacter(codepoint);
}
void ImGuiForVulkan::initInput() {
    active = this;
    previousKey = glfwSetKeyCallback(window, [](GLFWwindow* w, int k, int s, int a, int m) {
        auto* self = active; if (!self || self->window != w) return;
        self->handleKey(k, s, a, m);
        if (self->previousKey) self->previousKey(w, k, s, a, m);
    });
    previousChar = glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int c) {
        auto* self = active; if (!self || self->window != w) return;
        self->charPressed(c);
        if (self->previousChar) self->previousChar(w, c);
    });
    previousMousePos = glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
        auto* self = active; if (!self || self->window != w) return;
        self->handleMousePos(static_cast<float>(x), static_cast<float>(y));
        if (self->previousMousePos) self->previousMousePos(w, x, y);
    });
    previousMouseButton = glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int b, int a, int m) {
        auto* self = active; if (!self || self->window != w) return;
        self->handleMouseButton(b, a == GLFW_PRESS);
        if (self->previousMouseButton) self->previousMouseButton(w, b, a, m);
    });
    previousScroll = glfwSetScrollCallback(window, [](GLFWwindow* w, double x, double y) {
        auto* self = active; if (!self || self->window != w) return;
        ImGui::SetCurrentContext(self->context);
        if (glfwGetInputMode(w, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
            ImGui::GetIO().AddMouseWheelEvent(static_cast<float>(x), static_cast<float>(y));
        if (self->previousScroll) self->previousScroll(w, x, y);
    });
    previousFocus = glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
        auto* self = active; if (!self || self->window != w) return;
        ImGui::SetCurrentContext(self->context);
        ImGui::GetIO().AddFocusEvent(focused != 0);
        if (self->previousFocus) self->previousFocus(w, focused);
    });
    auto& platform = ImGui::GetPlatformIO();
    platform.Platform_ClipboardUserData = window;
    platform.Platform_GetClipboardTextFn = [](ImGuiContext*) { return glfwGetClipboardString(active->window); };
    platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) { glfwSetClipboardString(active->window, text); };
    inputInstalled = true;
}
void ImGuiForVulkan::updateInput() {
    auto& io = ImGui::GetIO();
    int w, h, fw, fh;
    glfwGetWindowSize(window, &w, &h); glfwGetFramebufferSize(window, &fw, &fh);
    io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    if (w > 0 && h > 0) io.DisplayFramebufferScale = ImVec2(static_cast<float>(fw) / w, static_cast<float>(fh) / h);
    const double now = glfwGetTime();
    io.DeltaTime = lastTime > 0 ? static_cast<float>(std::max(now - lastTime, 0.000001)) : 1.0f / 60;
    lastTime = now;
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        io.ClearInputKeys();
    } else if (glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
        double x, y; glfwGetCursorPos(window, &x, &y);
        io.AddMousePosEvent(static_cast<float>(x), static_cast<float>(y));
    }
}
void ImGuiForVulkan::shutdownInput() noexcept {
    if (!inputInstalled) return;
    glfwSetKeyCallback(window, previousKey); glfwSetCharCallback(window, previousChar);
    glfwSetCursorPosCallback(window, previousMousePos); glfwSetMouseButtonCallback(window, previousMouseButton);
    glfwSetScrollCallback(window, previousScroll); glfwSetWindowFocusCallback(window, previousFocus);
    inputInstalled = false; active = nullptr;
}
void ImGuiForVulkan::shutdown() noexcept {
    if (device) vkDeviceWaitIdle(device);
    shutdownInput();
    for (auto& frame : frames) {
        destroyBuffer(frame.vertices); destroyBuffer(frame.indices);
        for (auto& b : frame.staging) destroyBuffer(b);
        for (auto& t : frame.retired) destroyTexture(t);
    }
    frames.clear();
    if (context) ImGui::SetCurrentContext(context);
    for (auto& [tex, gpu] : textures) {
        destroyTexture(gpu);
        tex->SetTexID(ImTextureID_Invalid); tex->SetStatus(ImTextureStatus_Destroyed);
    }
    textures.clear();
    if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
    if (pipelineCache) vkDestroyPipelineCache(device, pipelineCache, nullptr);
    if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    if (descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    if (sampler) vkDestroySampler(device, sampler, nullptr);
    pipeline = VK_NULL_HANDLE; pipelineCache = VK_NULL_HANDLE; pipelineLayout = VK_NULL_HANDLE;
    descriptorPool = VK_NULL_HANDLE; descriptorSetLayout = VK_NULL_HANDLE; sampler = VK_NULL_HANDLE;
    if (context) ImGui::DestroyContext(context);
    context = nullptr; device = VK_NULL_HANDLE; window = nullptr; lastTime = 0;
}
