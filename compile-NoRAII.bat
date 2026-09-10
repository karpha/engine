D:/vulkan/vulkanSDK/SDK/Bin/glslc.exe ./shaders/shader.vert -o ./shaders/vert.spv
D:/vulkan/vulkanSDK/SDK/Bin/glslc.exe ./shaders/shader.frag -o ./shaders/frag.spv
D:/vulkan/vulkanSDK/SDK/Bin/slangc.exe ./shaders/pbrLightVert.slang -entry VSMain -target spirv -o ./shaders/pbrVert.spv
D:/vulkan/vulkanSDK/SDK/Bin/slangc.exe ./shaders/pbrLightFrag.slang -entry PSMain -target spirv -o ./shaders/pbrFrag.spv
pause