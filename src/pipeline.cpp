#include "pipeline.h"
#include "buffer.h"
#include "descriptor.h"
#include "device.h"     // 前向声明后，如果需要用到对应类的功能函数，需要在cpp中添加前向声明的头文件

#include <fstream>
#include <stdexcept>


Pipeline::Pipeline(Device* dev, Descriptor* dscrp, RenderPass* rp, bool usePBR) {
    device = dev;
    descriptor = dscrp;
    renderpass = rp;
    if (usePBR){
        pbrPipeline pbr;
        pbr.createGraphicsPipeline();
    } else {
        createGraphicsPipeline();
        std:: cout << "\tpipeline : constructor\n";
    }
}

Pipeline::~Pipeline() {
    if (graphicsPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device->getDevice(), graphicsPipeline, nullptr);
    if (pipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device->getDevice(), pipelineLayout, nullptr);
    std::cout << "\tpipeline: destructor\n";
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

void Pipeline::createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = device->getMsaaSamples();

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    VkDescriptorSetLayout descriptorSetLayout = descriptor->getDescriptorSetLayout();   // 不能直接用 “&descriptor->get()”，？
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderpass->getRenderpass();
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device->getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(device->getDevice(), vertShaderModule, nullptr);
}

bool pbrPipeline::createPBRPipeline(){
    try
    {
        /* code */
        auto shaderCode = readFile("shaders/pbr.spv");      // spv contains both shader and fragment shader code
        VkShaderModule pbrShaderModule;
        vkCreateShaderModule(device->getDevice(), shaderModuleCreateInfo, nullptr, &pbrShaderModule);

        VkPipelineShaderStageCreateInfo pbrVertexShaderStageInfo{};
        pbrVertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        pbrVertexShaderStageInfo.module = pbrShaderModule;
        pbrVertexShaderStageInfo.pName = "VSMain";

        VkPipelineShaderStageCreateInfo pbrFragmentShaderStageInfo{};
        pbrFragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pbrFragmentShaderStageInfo.module = pbrShaderModule;
        pbrFragmentShaderStageInfo.pName = "PSMain";

        VkPipelineShaderStageCreateInfo pbrShaderStages[] = { pbrVertexShaderStageInfo, pbrFragmentShaderStageInfo };

        VkPipelineVertexInputStateCreateInfo pbrVertexInputInfo{};
        VkVertexInputBindingDescription bindingDescription;
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(float) * 14;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions;
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;
        
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = sizeof(float) * 3;
        
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = sizeof(float) * 6;
        
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = sizeof(float) * 8;
        
        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[4].offset = sizeof(float) * 12;       // 为什么是12
        pbrVertexInputInfo.vertexBindingDescriptionCount = 1;
        pbrVertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        pbrVertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescriptions.size());
        pbrVertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo pbrInputAssmbly;
        pbrInputAssmbly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pbrInputAssmbly.primitiveRestartEnable = false;
        
        VkPipelineViewportStateCreateInfo pbrViewPortState;
        pbrViewPortState.viewportCount = 1;
        pbrViewPortState.scissorCount = 1;
        std::vector<VkDynamicState> pbrDynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo pbrDynamicState;
        pbrDynamicState.dynamicStateCount = static_cast<uint32_t>(pbrDynamicStates.size());
        pbrDynamicState.pDynamicStates = pbrDynamicStates.data();

        VkPipelineRasterizationStateCreateInfo pbrRastrizer;
        pbrRastrizer.depthClampEnable = false;
        pbrRastrizer.rasterizerDiscardEnable = false;
        pbrRastrizer.polygonMode = VK_POLYGON_MODE_FILL;
        pbrRastrizer.lineWidth = 1.0f;
        pbrRastrizer.cullMode = VK_CULL_MODE_BACK_BIT;
        pbrRastrizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pbrRastrizer.depthBiasEnable = false;

        VkPipelineDepthStencilStateCreateInfo pbrDepthStencil;
        pbrDepthStencil.depthTestEnable = true;
        pbrDepthStencil.depthWriteEnable = true;
        pbrDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        pbrDepthStencil.depthBoundsTestEnable = false;
        pbrDepthStencil.stencilTestEnable = false;

        VkPipelineColorBlendAttachmentState pbrColorBlendAttachment;
        pbrColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        pbrColorBlendAttachment.blendEnable = true;
        pbrColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pbrColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pbrColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        pbrColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        pbrColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        pbrColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        VkPipelineColorBlendStateCreateInfo pbrColorBlend;
        pbrColorBlend.logicOpEnable = false;
        pbrColorBlend.attachmentCount = 1;
        pbrColorBlend.pAttachments = &pbrColorBlendAttachment;

        VkPushConstantRange pbrPushConstantRange;
        pbrPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pbrPushConstantRange.offset = 0;
        pbrPushConstantRange.size = sizeof(PushConstantBlock);

        VkPipelineLayoutCreateInfo pbrPipelineLayoutInfo;
        pbrPipelineLayoutInfo.pushConstantRangeCount = 1;
        pbrPipelineLayoutInfo.pPushConstantRanges = &pbrPushConstantRange;
        pbrPipelineLayoutInfo.setLayoutCount = 1;
        pbrPipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout;

        auto pbrPipelineLayout = createPipelineLayout(pbrPipelineLayoutInfo);

        // pbr pipeline create👇
        VkGraphicsPipelineCreateInfo pbrPipelineInfo;
        pbrPipelineInfo.basePipelineHandle = nullptr;
        pbrPipelineInfo.layout = &pbrPipelineLayout;
        pbrPipelineInfo.pColorBlendState = &pbrColorBlend;
        pbrPipelineInfo.pDepthStencilState = &pbrDepthStencil;
        pbrPipelineInfo.pDynamicState = &pbrDynamicState;
        pbrPipelineInfo.pInputAssemblyState = &pbrInputAssmbly;
        pbrPipelineInfo.pMultisampleState = &pbrMultisampling;
        pbrPipelineInfo.pRasterizationState = &pbrRastrizer;
        pbrPipelineInfo.pStages = pbrShaderStages.data();
        pbrPipelineInfo.pVertexInputState = &pbrVertexInputInfo;
        pbrPipelineInfo.pViewportState = &pbrViewPortState;
        pbrPipelineInfo.renderPass = nullptr;
        pbrPipelineInfo.stageCount = static_cast<uint32_t> (pbrShaderStages.size());
        pbrPipelineInfo.subpass = 0;

        VkPipelineRenderingCreateInfo pbrRenderingInfo;
        pbrRenderingInfo.colorAttachmentCount = 1;
        pbrRenderingInfo.depthAttachmentFormat = findDepthFormat();
        pbrRenderingInfo.pColorAttachmentFormats = &swapchainImageFormat;
        
        pbrPipelineInfo.pNext = &pbrRenderingInfo;
        pbrPipeline = createGraphicsPipeline(nullptr, pipelineInfo );
        
        return true;
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error creating PBR pipeline " << e.what() << '\n';
        return false;
    }
    
}


// pbr : update uniform buffer 👇
void pbrUpdateUniformBuffer( uint32_t currentFrame , Camera* camera){
    // auto transform = 
    UniformBufferObject pbrUBO{};
    pbrUBO.model = transform->GetmodelMatrix();
    if (camera){
        pbrUBO.view = camera->getViewMatrix();
        pbrUBO.proj = camera->getProjectionMatrix();
    } else
    {
        /* code */
        // use default view & proj matrix if no camera  
        pbrUBO.view = glm::lookAt(glm::vec3(2.0f,2.0f,2.0f), glm::vec3(0.0f,0.0f,0.0f), glm::vec3(0.0f, 0.0f, 1.0f) );
        pbrUBO.proj = glm::perspective( glm::radians(45.0f), swapchain.getSwapchainExtent().width / (float)swapchain.getSwapchainExtent().height,0.1f, 100.0f );
        pbrUBO.proj[1][1] *= -1;
    }
    // set up lights
    // white light from above
    pbrUBO.lightPositions[0] = glm::vec4(0.0f, 5.0f, 5.0f, 1.0f);
    pbrUBO.lightColors[0] = glm::vec4(300.0f, 300.0f, 300.0f, 1.0f);
    // blue light from left
    pbrUBO.lightPositions[1] = glm::vec4(-5.0f, 0.0f, 0.0f, 1.0f);
    pbrUBO.lioghtColors[1] = glm::vec4(0.0f, 0.0f, 300.0f, 1.0f);
    // red light from right
    pbrUBO.lightPositions[2] = glm::vec4(5.0f, 0.0f, 0.0f, 1.0f);
    pbrUBO.lioghtColors[2] = glm::vec4(300.0f, 0.0f, 0.0f, 1.0f);
    // green light from behind
    pbrUBO.lightPositions[3] = glm::vec4(-5.0f, 0.0f, 0.0f, 1.0f);
    pbrUBO.lioghtColors[3] = glm::vec4(0.0f, 0.0f, 300.0f, 1.0f);
    pbrUBO.camPos = glm::vec4( camera ? camera->getPosition() : glm::vec3(2.0f,2.0f,2.0f), 1.0f );

    // pbr parameter
    pbrUBO.exposure = 4.5f;
    pbrUBO.gamma = 2.2f;
    pbrUBO.prefilteredCubeMipLevels = 1.0f;
    pbrUBO.scaleIBLAmbient = 1.0f;

    memcpy(uniformBuffers[currentFrame].mapped, &pbrUBO, sizeof(pbrUBO));

}
// pbr : update uniform buffer 👆
// pbr : push material properties to shader 👇
void pushMaterialProperties(VkCommandBuffer commandBuffer, const Model* model, uint32_t materialIndex){
    // get material from model
    const Material& material = model->materials[materialIndex];

    // define pbr push constants
    pbrPipeline::pbrPushConstantBlock pbrPushConstants{};
    pbrPushConstants.baseColorFactor = material.baseColorFactor;
    pbrPushConstants.metallicFactor = material.metallicFactor;
    pbrPushConstants.roughnessFactor = material.roughness;
    pbrPushConstants.baseColorTextureSet = material.baseColorTextureIndex;
    pbrPushConstants.physicalDescriptorTextureSet = material.metallicRoughnessTextureIndex;
    pbrPushConstants.normalTextureSet = material.normalTextureIndex;
    pbrPushConstants.occlusionTextureSet = material.occlusionTextureIndex;
    pbrPushConstants.emissiveTextureSet = material.emissiveTextureIndex;
    pbrPushConstants.alphaMask = material.alphaMode == AlphaMode::MASK ? 1.0f : 0.0f;
    pbrPushConstants.alphaMaskCutoff = material.alphaCutoff;

    // push constants to shader
    commandBuffer.pushConstants(*pbrPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pbrPushConstantBlock), &pbrPushConstants);
}
// pbr : push material properties to shader 👆

void  pbrPipeline::pbrRenderTest() {
    // Set up camera
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Set up lights
    // Light 1: White light from above
    glm::vec4 lightPos1 = glm::vec4(0.0f, 5.0f, 5.0f, 1.0f);
    glm::vec4 lightColor1 = glm::vec4(300.0f, 300.0f, 300.0f, 1.0f);

    // Light 2: Blue light from the left
    glm::vec4 lightPos2 = glm::vec4(-5.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 lightColor2 = glm::vec4(0.0f, 0.0f, 300.0f, 1.0f);

    // Load glTF models
    Model* damagedHelmet = modelLoader.loadModel("models/DamagedHelmet.gltf");
    Model* flightHelmet = modelLoader.loadModel("models/FlightHelmet.gltf");

    // The models already have PBR materials defined in the glTF file
    // We can render them directly with our PBR pipeline

    // Render the models with different transformations
    renderModel(damagedHelmet, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.5f));
    renderModel(flightHelmet, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.5f));

    // We can also experiment with modifying the material properties
    // For example, to make the damaged helmet more metallic:
    if (damagedHelmet->materials.size() > 0) {
        // Store the original value to restore later
        float originalMetallic = damagedHelmet->materials[0].metallicFactor;

        // Modify the material
        damagedHelmet->materials[0].metallicFactor = 1.0f;

        // Render with modified material
        renderModel(damagedHelmet, glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(0.5f));

        // Restore original value
        damagedHelmet->materials[0].metallicFactor = originalMetallic;
    }
}