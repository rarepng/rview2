#pragma once
#include "core/rvk.hpp"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
class ubo {
public:
	static bool init(rvkbucket &mvkobjs, std::vector<ubodata> &ubodata);
	static bool createlayout(rvkbucket &mvkobjs, VkDescriptorSetLayout& dlayout);
	static void upload(rvkbucket &mvkobjs, std::vector<ubodata> &ubodata, std::vector<glm::mat4> mats);
	static void cleanup(rvkbucket &mvkobjs, std::vector<ubodata> &ubodata);
};
