###### 文件结构
> 是否需要注重RAII？
- main.cpp 程序入口，创建窗口，创建引擎，渲染器renderer对象，主循环run（）， 释放资源
- 有device.cpp 选择物理设备，创建逻辑设备，队列，检查vulkan扩展、设备功能、交换链是否支持等
- 有pipeline.cpp 负责创建图形管线，包括shader，顶点输入，viewport/scissor配置，pipeline layout配置，
- 有scene loading，负责加载场景，模型资源，从文件中读取texture，meterial，mesh，camera，lighting等
- 有model.cpp， 负责顶点数据、索引数据、
- 有buffer.cpp，管理各类buffer分配，绑定数据，上传数据，
- 有descriptor.cpp ，负责descriptor set相关
- 有texture.cpp， 负责纹理资源加载与创建，包括图片解码，vulkan image， image view， sampler等
- 有shader.cpp， 负责加载编译shader，生成shader module
- 有swapchain.cpp， 创建 和 管理交换链
- 有command.cpp， 负责command pool与command buffer
- 有renderer
- 有window.cpp， 负责创建窗口，处理窗口大小变化，键盘输入
- 有vulkan instance.cpp， 用于创建vulkan 实例，

- 有camera.cpp， 负责相机
- 有utils.cpp， 主要是辅助函数

> 像这样程序中有上述完整的绘制图像的过程的（包括加载texture和model），这样的一个main文件可以认为是一个渲染器（renderer），但是不足以被认为是一个完整的引擎（engine）
-   在使用get()函数获取类的私有成员时，如果成员是一个结构体类型，包含数据过多时，可以在get()函数前添加 "&"，返回成员的引用。
- 使用vcpkg安装所需依赖之后，使用cmake -S . -B sdfsd 命令时为：
```bash
cmake -S . -B build  -G "Visual Studio 17 2022" -A X64 -DCMAKE_TOOLCHAIN_FILE="D:\vcpkg\vcpkg\scripts\buildsystems\vcpkg.cmake" 
cmake --build build --config Debug

- 需要添加vcpkg路径方能有效识别【出错时将已有build目录删除重新执行】,
```bash
cmake -S . -B build_engine -G "Visual Studio 16 2019" -A x64 -DCMAKE_TOOLCHAIN_FILE=F:\vcpkgRebuild\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build_engine --config Debug

- 传参数时，是用变量（复制一份）传入还是引用（&）传入，还是传变量的指针。

##### 使用RenderDoc做调试与性能调整
> 光照模型，pbr

### PBR renderer

PBR C++ implementation and GPU layouts live in `pbr.h` / `pbr.cpp`.
The existing Slang shaders implement GGX / Smith / Schlick direct lighting.
`main.cpp` creates Engine and configures the demo; engine.cpp owns the frame loop and Vulkan submission.
`Buffer` and `Texture` supply the existing allocation/upload helpers.

Requirements: Vulkan 1.1 device, Vulkan SDK with slangc, and the vcpkg manifest
(including the pinned tinygltf dependency). No manual SPIR-V compilation
is needed: the PbrShaders CMake target builds both stages with column-major matrices.

From the repository root, after configuring the existing build directory:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
./build/Debug/Engine.exe
./build/Debug/Engine.exe --legacy
./build/Debug/Engine.exe --frames 120
./build/Debug/Engine.exe --validate-pbr
```

Default scene: DamagedHelmet and FlightHelmet, fitted side by side. Camera controls
are unchanged. Engine owns scene objects and exposes loadModel, createObject,
setTransform, removeObject, clearObjects, setLight and setExposure. PbrRenderer
receives the object list after waiting on the frame fence.

Supported: static triangle glTF/glb scenes, node transforms, uint8/16/32 indices,
accessor offsets/strides and normalized attributes, generated missing normals and
tangents, five material maps, mipmaps, glTF sampler wrapping/filtering, sRGB color
maps, alpha mask/blend, double-sided materials and mirrored transforms. Transparent
primitives are sorted by their centers (intersecting transparent geometry may need
more advanced sorting). A default material and fallback maps cover missing inputs.

Limits: one UV set (TEXCOORD_0); no sparse accessors, skinning, morph targets or
required extensions. Optional material extensions use the core material fallback,
with a console message. FlightHelmet's transmission lens therefore has no physical
transmission yet. Ambient light is a constant approximation, not IBL; there are no
shadow maps. Existing swapchain recreation assumes the attachment format stays
compatible with the render pass. Tests exercise the actual CPU glTF loader, bounds,
accessor normalization/stride handling, and generated normal/tangent bases.

### Minimal rendering engine

Only `engine.h` and `engine.cpp` are new. Existing Vulkan wrappers, camera, UI,
shaders and legacy rendering path remain in use. Engine contains the frame loop,
Vulkan resource lifetime, a canonical-path model cache and a small object list.
There are no separate Scene, AssetManager, Input or Application modules.

Model resources are shared; objects have independent transforms and stable IDs.
Removing objects does not unload cached models. Resources live until engine
shutdown. Runtime loading is synchronous and waits for GPU idle. Per-frame
instance uniforms/descriptors are replaced only after that frame's fence signals.
Object matrices must be finite, affine and nonsingular. This is a rendering
engine, without JSON persistence, hierarchy, physics, audio or an editor.
The existing four-point-light and static-mesh limits still apply.

```cpp
Engine engine;
auto model = engine.loadModel("models/DamagedHelmet/DamagedHelmet.gltf");
auto object = engine.createObject(model, engine.fitTransform(model, {0,1,0}, 1.5f));
// Another createObject(model, ...) shares mesh/texture GPU allocations.
engine.run([&](Engine& engine, float dt) {
    // Application update: engine.setTransform(object, matrix), removeObject, etc.
});
```

Build/run commands above are unchanged. `--scene` and `--save-scene` from the
larger experimental refactor are no longer supported. Ordinary window resize is
supported; changing swapchain attachment format requires restarting the process.
