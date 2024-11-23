#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"

class gltfmeshpipeline {
public:
	static bool init(vkobjs& objs, VkPipelineLayout& playout, VkPipeline& pipeline, VkPrimitiveTopology topology, std::string tname, std::string mname,std::string fname);
	static void cleanup(vkobjs& objs, VkPipeline& pipeline);
};