#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>
// #include "tinygltf/tiny_gltf.h"
#include <fastgltf/core.hpp>
class vktexture {
public:
	static bool loadtexture(rvkbucket &rdata, std::vector<texdata> &texdata, fastgltf::Asset &mmodel);
	static bool loadtexset(rvkbucket &rdata, std::vector<texdata> &texdata, VkDescriptorSetLayout &dlayout,VkDescriptorSet &dset,
	                       fastgltf::Asset &mmodel);
	static bool createlayout(rvkbucket &core);
	static void cleanup(rvkbucket &rdata, texdata &texdata);
	// static void cleanuppls(rvkbucket &rdata, texdataset &texdatapls);
};
