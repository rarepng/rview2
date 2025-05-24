#pragma once
#include "vkobjs.hpp"
#include <vulkan/vulkan.h>
// #include "tinygltf/tiny_gltf.h"
#include <fastgltf/core.hpp>
class vktexture {
public:
	static bool loadtexturefile(vkobjs &rdata, texdata &texdata, texdatapls &texdatapls, std::string texfile);
	static bool loadtexture(vkobjs &rdata, std::vector<texdata> &texdata, fastgltf::Asset &mmodel);
	static bool loadtexlayoutpool(vkobjs &rdata, std::vector<texdata> &texdata, texdatapls &texdatapls,
	                              fastgltf::Asset &mmodel);
	static void cleanup(vkobjs &rdata, texdata &texdata);
	static void cleanuppls(vkobjs &rdata, texdatapls &texdatapls);
};
