cmake_minimum_required(VERSION 3.16)
project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== 依赖查找 (vcpkg manifest 模式) =====
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(stb REQUIRED)
find_package(imgui CONFIG REQUIRED)

# ===== 可执行文件 =====
add_executable(Engine 
    src/main.cpp
    src/instance.cpp
    src/window.cpp
    src/device.cpp
    src/swapchain.cpp
    src/renderpass.cpp
    src/buffer.cpp
    src/command.cpp
    src/descriptor.cpp
    src/texture.cpp
    src/thirdParty.cpp
    src/loadmodel.cpp
    src/pipeline.cpp
    src/other.cpp
    src/camera.cpp
    src/pbr.cpp
    src/ImGui.cpp
)

target_link_libraries(Engine PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    tinyobjloader::tinyobjloader
    imgui::imgui
)
# Resolve the package header explicitly: src/ImGui.h has the same name on Windows.
find_path(IMGUI_CORE_INCLUDE_DIR imgui.h REQUIRED)
target_compile_definitions(Engine PRIVATE ENGINE_IMGUI_CORE_HEADER="${IMGUI_CORE_INCLUDE_DIR}/imgui.h")
target_include_directories(Engine PRIVATE ${STB_INCLUDE_DIR})
# tinygltf is a header-only vcpkg dependency; its implementation lives in pbr.cpp.
find_path(TINYGLTF_INCLUDE_DIR tiny_gltf.h)
if(NOT TINYGLTF_INCLUDE_DIR)
    message(FATAL_ERROR "tinygltf is missing. Install the dependencies in vcpkg.json.")
endif()
target_include_directories(Engine PRIVATE ${TINYGLTF_INCLUDE_DIR})
target_compile_definitions(Engine PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
if(MSVC)
    target_compile_options(Engine PRIVATE /utf-8)
endif()

find_program(SLANGC_EXECUTABLE NAMES slangc HINTS "$ENV{VULKAN_SDK}/Bin")
if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc is required to compile the PBR shaders (Vulkan SDK).")
endif()
set(PBR_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${PBR_SHADER_OUTPUT_DIR}")
foreach(STAGE IN ITEMS vert frag)
    if(STAGE STREQUAL "vert")
        set(SHADER_SOURCE pbrLightVert.slang)
        set(SHADER_ENTRY VSMain)
        set(SHADER_STAGE vertex)
    else()
        set(SHADER_SOURCE pbrLightFrag.slang)
        set(SHADER_ENTRY PSMain)
        set(SHADER_STAGE fragment)
    endif()
    add_custom_command(
        OUTPUT "${PBR_SHADER_OUTPUT_DIR}/pbr.${STAGE}.spv"
        COMMAND "${SLANGC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_SOURCE}"
            -entry ${SHADER_ENTRY} -stage ${SHADER_STAGE} -target spirv -profile spirv_1_3
            -emit-spirv-directly -fvk-use-entrypoint-name -matrix-layout-column-major
            -o "${PBR_SHADER_OUTPUT_DIR}/pbr.${STAGE}.spv"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER_SOURCE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pbrCommon.slang"
        VERBATIM)
endforeach()
add_custom_target(PbrShaders DEPENDS
    "${PBR_SHADER_OUTPUT_DIR}/pbr.vert.spv" "${PBR_SHADER_OUTPUT_DIR}/pbr.frag.spv")
add_dependencies(Engine PbrShaders)
target_compile_definitions(Engine PRIVATE PBR_SHADER_DIR="${PBR_SHADER_OUTPUT_DIR}")
set_property(TARGET Engine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
include(CTest)
if(BUILD_TESTING)
    add_test(NAME PbrAssets COMMAND Engine --validate-pbr)
    set_tests_properties(PbrAssets PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# GUI shader implementation stays in ImGui.cpp; generated files live in build/.
if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR "glslangValidator is required to compile the embedded ImGui shaders.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui.cpp")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui.cpp" IMGUI_IMPLEMENTATION)
foreach(STAGE IN ITEMS vertex fragment)
    string(REGEX MATCH "R\"glsl_${STAGE}\\(([^@]*)\\)glsl_${STAGE}\"" SHADER_MATCH "${IMGUI_IMPLEMENTATION}")
    if(NOT SHADER_MATCH)
        message(FATAL_ERROR "Cannot find embedded ImGui ${STAGE} shader")
    endif()
    if(STAGE STREQUAL "vertex")
        set(SUFFIX vert)
    else()
        set(SUFFIX frag)
    endif()
    set(GUI_SOURCE "${PBR_SHADER_OUTPUT_DIR}/imgui.${SUFFIX}")
    file(WRITE "${GUI_SOURCE}" "${CMAKE_MATCH_1}")
    add_custom_command(OUTPUT "${GUI_SOURCE}.spv"
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V "${GUI_SOURCE}" -o "${GUI_SOURCE}.spv"
        DEPENDS "${GUI_SOURCE}" VERBATIM)
endforeach()
add_custom_target(ImGuiShaders DEPENDS "${PBR_SHADER_OUTPUT_DIR}/imgui.vert.spv" "${PBR_SHADER_OUTPUT_DIR}/imgui.frag.spv")
add_dependencies(Engine ImGuiShaders)
target_compile_definitions(Engine PRIVATE IMGUI_SHADER_DIR="${PBR_SHADER_OUTPUT_DIR}")
