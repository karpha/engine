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
- 有camera.cpp， 负责相机
- 有window.cpp， 负责创建窗口，处理窗口大小变化，键盘输入
- 有utils.cpp， 主要是辅助函数
- 有vulkan instance.cpp， 用于创建vulkan 实例，

> 像这样程序中有上述完整的绘制图像的过程的（包括加载texture和model），这样的一个main文件可以认为是一个渲染器（renderer），但是不足以被认为是一个完整的引擎（engine）
-   在使用get()函数获取类的私有成员时，如果成员是一个结构体类型，包含数据过多时，可以在get()函数前添加 "&"，返回成员的引用。