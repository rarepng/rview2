#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>
// #include "tinygltf/tiny_gltf.h"
#include <fastgltf/core.hpp>
class vktexture {
public:
	static bool loadtexturefile(rvk &rdata, texdata &texdata, texdatapls &texdatapls, std::string texfile);
	static bool loadtexture(rvk &rdata, std::vector<texdata> &texdata, fastgltf::Asset &mmodel);
	static bool loadtexlayoutpool(rvk &rdata, std::vector<texdata> &texdata, texdatapls &texdatapls,
	                              fastgltf::Asset &mmodel);
	static void cleanup(rvk &rdata, texdata &texdata);
	static void cleanuppls(rvk &rdata, texdatapls &texdatapls);
};
