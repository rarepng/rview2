#pragma once
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
#include "tinygltf/tiny_gltf.h"
class vktexture {
public:
	static bool loadtexturefile(vkobjs& rdata,vktexdata& texdata, std::string texfile);
	static bool loadtexture(vkobjs& rdata, std::vector<vktexdata>& texdata, std::shared_ptr<tinygltf::Model> mmodel);
	static bool loadtexlayoutpool(vkobjs& rdata, std::vector<vktexdata>& texdata, std::shared_ptr<tinygltf::Model> mmodel);
	static void cleanup(vkobjs& rdata, vktexdata& texdata);
};