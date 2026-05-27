#include "playout.hpp"

#include "VkBootstrap.h"

bool playout::init(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout, std::span<VkDescriptorSetLayout> layoutz,
                   size_t pushc_size) {


	VkPushConstantRange pCs{};
	pCs.offset = 0;
	pCs.size = pushc_size;
	pCs.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo plinfo{};
	plinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plinfo.setLayoutCount = layoutz.size();
	plinfo.pSetLayouts = layoutz.data();
	if (pushc_size > 0) {
		plinfo.pushConstantRangeCount = 1;
		plinfo.pPushConstantRanges = &pCs;
	} else {
		plinfo.pushConstantRangeCount = 0;
		plinfo.pPushConstantRanges = nullptr;
	}
	plinfo.pPushConstantRanges = &pCs;
	if (vkCreatePipelineLayout(mvkobjs.vkdevice.device, &plinfo, nullptr, &vkplayout) != VK_SUCCESS)
		return false;
	return true;
}
bool playout::init_bindless(rvkbucket& objs) {
    constexpr uint32_t MAX_BINDLESS = 1024;
    for (uint32_t i = 1; i < rvkbucket::MAX_BINDLESS_BUFFERS; ++i) {
        objs.global_buffers.free_slots.push(i);
    }
    std::array<VkDescriptorSetLayoutBinding, 7> bindings{};

    // tex
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = MAX_BINDLESS;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

    // ubo (cam)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // ssbo (joints)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = MAX_BINDLESS;
    bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // env
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // ssbo (mats)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // vbo
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = MAX_BINDLESS;
    bindings[5].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // ebo
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[6].descriptorCount = MAX_BINDLESS;
    bindings[6].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::array<VkDescriptorBindingFlags, 7> flags{};
    VkDescriptorBindingFlags bindlessFlags = 
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | 
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    flags[0] = bindlessFlags;
    flags[1] = 0;
    flags[2] = bindlessFlags;
    flags[3] = 0;
    flags[4] = 0;
    flags[5] = bindlessFlags;
    flags[6] = bindlessFlags;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flagsInfo.bindingCount = static_cast<uint32_t>(flags.size());
    flagsInfo.pBindingFlags = flags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.pNext = &flagsInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(objs.vkdevice.device, &layoutInfo, nullptr, &rvkbucket::globalBindlessLayout) != VK_SUCCESS)
        return false;

    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = MAX_BINDLESS + 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = (MAX_BINDLESS * 3) + 2; // joints + mats + vbo + ebo

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(objs.vkdevice.device, &poolInfo, nullptr, &rvkbucket::globalBindlessPool) != VK_SUCCESS)
        return false;

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = rvkbucket::globalBindlessPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &rvkbucket::globalBindlessLayout;

    if (vkAllocateDescriptorSets(objs.vkdevice.device, &allocInfo, &rvkbucket::globalBindlessSet) != VK_SUCCESS)
        return false;

	VkPushConstantRange pCs{};
	pCs.offset = 0;
	pCs.size = sizeof(vkpushconstants);
	pCs.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo plinfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	plinfo.setLayoutCount = 1;
	plinfo.pSetLayouts = &rvkbucket::globalBindlessLayout;
	plinfo.pushConstantRangeCount = 1;
	plinfo.pPushConstantRanges = &pCs;

	if (vkCreatePipelineLayout(objs.vkdevice.device, &plinfo, nullptr, &rvkbucket::globalPipelineLayout) != VK_SUCCESS) {
		return false;
	}

    return true;
}

void playout::cleanup(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout) {
	vkDestroyPipelineLayout(mvkobjs.vkdevice.device, vkplayout, nullptr);
}
