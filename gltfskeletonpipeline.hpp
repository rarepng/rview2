#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"

class gltfskeletonpipeline {
public:
	static bool init(vkobjs& objs, VkPipelineLayout& playout, VkPipeline& pipeline, VkPrimitiveTopology topology, std::string vname, std::string fname);
	static void cleanup(vkobjs& objs, VkPipeline& pipeline);
};