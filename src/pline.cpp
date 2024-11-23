#include <vector>

#include "pline.hpp"
#include "vkshader.hpp"

#include <glm/glm.hpp>
#include <vk/VkBootstrap.h>

bool pline::init(vkobjs& objs, VkPipelineLayout& playout, VkPipeline& pipeline, VkPrimitiveTopology topology, unsigned int v_in,unsigned int atts, std::vector<std::string> sfiles,bool char_or_short ){
    if (sfiles.size() < 2)return false;
    std::vector<VkShaderModule> shaders;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfo;
    shaders.reserve(sfiles.size());
    shaders.resize(sfiles.size());
    shaderStageInfo.reserve(sfiles.size());
    shaderStageInfo.resize(sfiles.size());
    for (size_t i{ 0 }; i < sfiles.size();i++) {
        shaders[i] = vkshader::loadshader(objs.rdvkbdevice.device, sfiles[i]);


        shaderStageInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        //shaderStageInfo[i].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStageInfo[i].module = shaders[i];
        shaderStageInfo[i].pName = "main";

    }
    shaderStageInfo.front().stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStageInfo.back().stage = VK_SHADER_STAGE_FRAGMENT_BIT;




    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector < VkVertexInputAttributeDescription> posnoruvjointweightattsout;
    if (atts & 1) {
        vertexBindings.push_back({ (unsigned int)vertexBindings.size(),sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX});
        posnoruvjointweightattsout.push_back({static_cast<unsigned int>(posnoruvjointweightattsout.size()),static_cast<unsigned int>(posnoruvjointweightattsout.size()),VK_FORMAT_R32G32B32_SFLOAT ,0});
    }
    if (atts & 2) {
        vertexBindings.push_back({ (unsigned int)vertexBindings.size(),sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX });
        posnoruvjointweightattsout.push_back({ static_cast<unsigned int>(posnoruvjointweightattsout.size()),static_cast<unsigned int>(posnoruvjointweightattsout.size()),VK_FORMAT_R32G32B32_SFLOAT ,0 });
    }
    if (atts & 4) {
        vertexBindings.push_back({ (unsigned int)vertexBindings.size(),sizeof(glm::vec2), VK_VERTEX_INPUT_RATE_VERTEX });
        posnoruvjointweightattsout.push_back({ static_cast<unsigned int>(posnoruvjointweightattsout.size()),static_cast<unsigned int>(posnoruvjointweightattsout.size()),VK_FORMAT_R32G32_SFLOAT ,0 });
    }
    if (atts & 8 && !char_or_short) {
        vertexBindings.push_back({ (unsigned int)vertexBindings.size(),sizeof(uint8_t) * 4, VK_VERTEX_INPUT_RATE_VERTEX });
        posnoruvjointweightattsout.push_back({ static_cast<unsigned int>(posnoruvjointweightattsout.size()),static_cast<unsigned int>(posnoruvjointweightattsout.size()),VK_FORMAT_R8G8B8A8_UINT ,0 });
    }else 
    if (atts & 8) {
        vertexBindings.push_back({ (unsigned int)vertexBindings.size(),sizeof(unsigned int) * 4, VK_VERTEX_INPUT_RATE_VERTEX });
        posnoruvjointweightattsout.push_back({ static_cast<unsigned int>(posnoruvjointweightattsout.size()),static_cast<unsigned int>(posnoruvjointweightattsout.size()),VK_FORMAT_R32G32B32A32_UINT ,0 });
    }
    //if (atts & 8) {
    //    vertexBindings.push_back({ (unsigned int)vertexBindings.size(),sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX });
    //    posnoruvjointweightattsout.push_back({ static_cast<unsigned int>(posnoruvjointweightattsout.size()),static_cast<unsigned int>(posnoruvjointweightattsout.size()),VK_FORMAT_R16G16B16A16_UINT ,0 });
    //}
    if (atts & 16) {
        vertexBindings.push_back({ (unsigned int)vertexBindings.size(),sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX });
        posnoruvjointweightattsout.push_back({ static_cast<unsigned int>(posnoruvjointweightattsout.size()),static_cast<unsigned int>(posnoruvjointweightattsout.size()),VK_FORMAT_R32G32B32A32_SFLOAT ,0 });
    }



    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = vertexBindings.size();
    vertexInputInfo.pVertexBindingDescriptions = vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = posnoruvjointweightattsout.size();
    vertexInputInfo.pVertexAttributeDescriptions = posnoruvjointweightattsout.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = topology;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(objs.rdvkbswapchain.extent.width);
    viewport.height = static_cast<float>(objs.rdvkbswapchain.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = objs.rdvkbswapchain.extent;



    VkPipelineViewportStateCreateInfo viewportStateInfo{};
    viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.pViewports = &viewport;
    viewportStateInfo.scissorCount = 1;
    viewportStateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
    rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerInfo.depthClampEnable = VK_FALSE;
    rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerInfo.lineWidth = 1.0f;
    rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizerInfo.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
    multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendingInfo{};
    colorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingInfo.logicOpEnable = VK_FALSE;
    colorBlendingInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendingInfo.attachmentCount = 1;
    colorBlendingInfo.pAttachments = &colorBlendAttachment;
    colorBlendingInfo.blendConstants[0] = 0.0f;
    colorBlendingInfo.blendConstants[1] = 0.0f;
    colorBlendingInfo.blendConstants[2] = 0.0f;
    colorBlendingInfo.blendConstants[3] = 0.0f;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.minDepthBounds = 0.0f;
    depthStencilInfo.maxDepthBounds = 1.0f;
    depthStencilInfo.stencilTestEnable = VK_FALSE;

    std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH };

    VkPipelineDynamicStateCreateInfo dynStatesInfo{};
    dynStatesInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynStatesInfo.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dynStatesInfo.pDynamicStates = dynStates.data();




    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = sfiles.size();
    pipelineCreateInfo.pStages = shaderStageInfo.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineCreateInfo.pViewportState = &viewportStateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
    pipelineCreateInfo.pMultisampleState = &multisamplingInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendingInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
    pipelineCreateInfo.pDynamicState = &dynStatesInfo;
    pipelineCreateInfo.layout = playout;
    pipelineCreateInfo.renderPass = objs.rdrenderpass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;





    if (vkCreateGraphicsPipelines(objs.rdvkbdevice.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(objs.rdvkbdevice.device, playout, nullptr);
        return false;
    }

    for (const auto& i : shaders) {
        vkDestroyShaderModule(objs.rdvkbdevice.device, i, nullptr);
    }


    return true;
}

void pline::cleanup(vkobjs& objs, VkPipeline& pipeline) {
    vkDestroyPipeline(objs.rdvkbdevice.device, pipeline, nullptr);
}
