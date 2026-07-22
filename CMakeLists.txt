cmake_minimum_required(VERSION 3.16)
project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== 依赖查找 (vcpkg manifest 模式) =====
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)

# ===== 可执行文件 =====
add_executable(Engine src/main.cpp)

target_link_libraries(Engine PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    tinyobjloader::tinyobjloader
)