#pragma once
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vector>
#include <unordered_map>

// Forward declarations used by this wrapper.
struct ImGuiContext;
struct ImTextureData;
class Instance;
class Device;
class SwapChain;
class RenderPass;
class Camera;
class PbrRenderer;

// Vulkan Tutorial / Building a Simple Engine / GUI. No ImGui backend is used.
// Window, device and render pass must outlive this object.
class ImGuiForVulkan {
public:
    ImGuiForVulkan() = default;
    ~ImGuiForVulkan();
    ImGuiForVulkan(const ImGuiForVulkan&) = delete;
    ImGuiForVulkan& operator=(const ImGuiForVulkan&) = delete;
    void init(GLFWwindow*, Instance&, Device&, SwapChain&, RenderPass&, uint32_t framesInFlight);
    void shutdown() noexcept;
    void newFrame(const Camera&, PbrRenderer*);
    // After the frame fence, during command recording, outside RenderPass.
    void prepareFrame(VkCommandBuffer, uint32_t frameIndex);
    // Last draw in the scene RenderPass; submission belongs to the renderer.
    void drawFrame(VkCommandBuffer);
    void onSwapChainRecreated(); // Requires device idle.
    void setStyle(uint32_t index);
    bool getWantKeyCapture() const;
    bool getWantMouseCapture() const;
    void handleKey(int key, int scancode, int action, int mods);
    void handleMousePos(float x, float y);
    void handleMouseButton(int button, bool pressed);
    void charPressed(uint32_t codepoint);

private:
    struct Buffer {
        VkBuffer handle = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        void* mapped = nullptr;
    };
    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet descriptor = VK_NULL_HANDLE;
    };
    struct Frame {
        Buffer vertices, indices;
        std::vector<Buffer> staging;
        std::vector<Texture> retired;
    };
    struct PushConstBlock { float scale[2]; float translate[2]; };
    void initResources();
    void createPipeline();
    void updateTexture(ImTextureData*, VkCommandBuffer, Frame&);
    void updateBuffers(Frame&);
    void bindRenderState(VkCommandBuffer);
    uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags) const;
    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    void destroyBuffer(Buffer&) noexcept;
    void destroyTexture(Texture&) noexcept;
    void initInput();
    void updateInput();
    void shutdownInput() noexcept;
    static ImGuiForVulkan* active;

    // Window and Vulkan rendering resources.
    GLFWwindow* window = nullptr;
    ImGuiContext* context = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    SwapChain* swapchain = nullptr;
    RenderPass* renderpass = nullptr;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    // Texture ownership, per-frame buffers and UI state.
    std::unordered_map<ImTextureData*, Texture> textures;
    std::vector<Frame> frames;
    uint32_t currentFrame = 0;
    double lastTime = 0;
    float exposure = 1.0f;
    int styleIndex = 0;
    bool showDemo = false;
    bool inputInstalled = false;
    // Original GLFW callbacks restored during shutdown.
    GLFWkeyfun previousKey = nullptr;
    GLFWcharfun previousChar = nullptr;
    GLFWcursorposfun previousMousePos = nullptr;
    GLFWmousebuttonfun previousMouseButton = nullptr;
    GLFWscrollfun previousScroll = nullptr;
    GLFWwindowfocusfun previousFocus = nullptr;
};
