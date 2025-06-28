#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>
// #include "tinygltf/tiny_gltf.h"
#include <fastgltf/core.hpp>
class vktexture {
public:
	static bool loadtexturefile(rvk &rdata, texdata &texdata, texdataset &texdatapls, std::string texfile);
	static bool loadtexture(rvk &rdata, std::vector<texdata> &texdata, fastgltf::Asset &mmodel);
	static bool loadtexlayoutpool(rvk &rdata, std::vector<texdata> &texdata, texdataset &texdatapls,
	                              fastgltf::Asset &mmodel);
	static void cleanup(rvk &rdata, texdata &texdata);
	static void cleanuppls(rvk &rdata, texdataset &texdatapls);
};
