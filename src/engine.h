#pragma once
#include "pbr.h"
#include <functional>
#include <memory>
#include <string>

struct EngineConfig {
    std::string title = "Vulkan Engine";
    int width = 800, height = 600;
    bool pbr = true;
    int frameLimit = 0;
};

// A small rendering engine built on the existing Vulkan wrappers.
// Models are shared resources; objects have independent transforms and stable IDs.
class Engine {
public:
    using ObjectId = uint64_t;
    using Update = std::function<void(Engine&, float)>;
    explicit Engine(EngineConfig config = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    ModelHandle loadModel(const std::string& path);
    glm::mat4 fitTransform(ModelHandle model, glm::vec3 center, float size) const;
    ObjectId createObject(ModelHandle model, const glm::mat4& transform = glm::mat4(1));
    void setTransform(ObjectId object, const glm::mat4& transform);
    void removeObject(ObjectId object);
    void clearObjects();
    size_t objectCount() const;
    size_t modelCount() const;
    void setLight(uint32_t index, glm::vec3 position, glm::vec3 intensity);
    void setExposure(float exposure);
    Camera& camera();

    // Default camera controls are preserved; update runs before each frame is drawn.
    void run(const Update& update = {});
    void requestClose();
    void resizeWindow(int width, int height);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
