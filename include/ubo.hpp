#pragma once
#include "core/rvk.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
namespace ubo {
	bool init(rvkbucket &mvkobjs, std::span<ubodata> ubodata);
	bool createlayout(rvkbucket &mvkobjs, VkDescriptorSetLayout& dlayout);
	void upload(rvkbucket &mvkobjs, std::vector<ubodata> &ubodata, std::vector<glm::mat4> mats,const glm::vec3& campos);
	void cleanup(rvkbucket &mvkobjs, std::vector<ubodata> &ubodata);
};
