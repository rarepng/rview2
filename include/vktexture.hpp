#pragma once
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
//#include "tinygltf/tiny_gltf.h"
#include <fastgltf/core.hpp>
class vktexture {
public:
	static bool loadtexturefile(vkobjs& rdata,vktexdata& texdata,vktexdatapls& texdatapls, std::string texfile);
	//static bool loadtexture(vkobjs& rdata, std::vector<vktexdata>& texdata, std::shared_ptr<tinygltf::Model> mmodel);
	//static bool loadtexlayoutpool(vkobjs& rdata, std::vector<vktexdata>& texdata, vktexdatapls& texdatapls, std::shared_ptr<tinygltf::Model> mmodel);
	static bool loadtexture(vkobjs& rdata, std::vector<vktexdata>& texdata, fastgltf::Asset& mmodel);
	static bool loadtexlayoutpool(vkobjs& rdata, std::vector<vktexdata>& texdata, vktexdatapls& texdatapls,fastgltf::Asset& mmodel);
	static void cleanup(vkobjs& rdata, vktexdata& texdata);
	static void cleanuppls(vkobjs& rdata, vktexdatapls& texdatapls);
};
