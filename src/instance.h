#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

class Instance{
public:
    // 遵循资源获取即初始化（RAII）原则时，需要在构造函数分配资源和内存，在析构函数释放/销毁资源和内存
    // Instance( const std::string& appName = "Hello Triangle", 
    //         const std::string& engineName = "No Engine",
    //         bool enableValidationLayers = true  );
    Instance( const std::string& appName, 
            const std::string& engineName ,
            bool enableValidationLayers );
    ~Instance();  
    // 禁用拷贝，防止双重释放
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    VkInstance getInstance() const {
        return instance;
    }
    bool isValidationLayerEnable(){
        return enableValidationLayers;
    }

    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    void populateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo );

    void setupDebugMessenger();
    void createSurface();
    VkSurfaceKHR getSurface();
    VkDebugUtilsMessengerEXT getDebugMessenger();
private:
    VkInstance instance;  
    bool enableValidationLayers;
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT debugMessenger;
};