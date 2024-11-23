#pragma once
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"

class meshplayout {
public:
	static bool init(vkobjs& mvkobjs, vktexdata& texdata, VkPipelineLayout& vkplayout);
	static void cleanup(vkobjs& mvkobjs, VkPipelineLayout& vkplayout);
};