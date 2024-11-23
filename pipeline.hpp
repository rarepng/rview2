#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
class pipeline {
public:
	static bool init(vkobjs& rdata,VkPipelineLayout& vkplayout,VkPipeline& vkpipe, VkPrimitiveTopology topology, std::string vshade, std::string fshade);
	static void cleanup(vkobjs& rdata, VkPipeline& vkpipe);
};