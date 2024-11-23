#include <vector>

#include "gltfmeshpipeline.hpp"
#include "vkshader.hpp"

#include <glm/glm.hpp>
#include <vk/VkBootstrap.h>

bool gltfmeshpipeline::init(vkobjs& objs, VkPipelineLayout& pipelineLayout,
    VkPipeline& pipeline, VkPrimitiveTopology topology,
    std::string tname, std::string mname,std::string fname) {
    VkShaderModule meshmod = vkshader::loadshader(objs.rdvkbdevice.device,
        mname);
    VkShaderModule taskmod = vkshader::loadshader(objs.rdvkbdevice.device,
        tname);
    VkShaderModule fragmod = vkshader::loadshader(objs.rdvkbdevice.device,
        fname);

    if (meshmod == VK_NULL_HANDLE || taskmod == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo tinfo{};
    tinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    tinfo.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    tinfo.module = taskmod;
    tinfo.pName = "main";

    VkPipelineShaderStageCreateInfo minfo{};
    minfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    minfo.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    minfo.module = meshmod;
    minfo.pName = "main";

    VkPipelineShaderStageCreateInfo finfo{};
    finfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    finfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    finfo.module = fragmod;
    finfo.pName = "main";


    VkPipelineShaderStageCreateInfo shaderStagesInfo[] = { tinfo,minfo,finfo };




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
    rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizerInfo.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisamplingInfo{};
    multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

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
    pipelineCreateInfo.stageCount = 3;
    pipelineCreateInfo.pStages = shaderStagesInfo;
    pipelineCreateInfo.pVertexInputState = nullptr;
    pipelineCreateInfo.pInputAssemblyState = nullptr;
    pipelineCreateInfo.pViewportState = &viewportStateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizerInfo;
    pipelineCreateInfo.pMultisampleState = &multisamplingInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendingInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilInfo;
    pipelineCreateInfo.pDynamicState = &dynStatesInfo;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = objs.rdrenderpass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(objs.rdvkbdevice.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(objs.rdvkbdevice.device, pipelineLayout, nullptr);
        return false;
    }

    vkDestroyShaderModule(objs.rdvkbdevice.device, fragmod, nullptr);
    vkDestroyShaderModule(objs.rdvkbdevice.device, meshmod, nullptr);
    vkDestroyShaderModule(objs.rdvkbdevice.device, taskmod, nullptr);

    return true;
}

void gltfmeshpipeline::cleanup(vkobjs& objs, VkPipeline& pipeline) {
    vkDestroyPipeline(objs.rdvkbdevice.device, pipeline, nullptr);
}
