#pragma once
#include <vulkan/vulkan.h>
#include <tinygltf/tiny_gltf.h>
#include "vkobjs.hpp"
class vkvbo {
public:
	static bool init(vkobjs& mvkobjs,vkvertexbufferdata& vbdata,unsigned int bsize);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, vkmesh vmesh);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, std::vector<glm::vec3> vertexdata);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, const tinygltf::Buffer& buffer,const tinygltf::BufferView& bufferview,const tinygltf::Accessor& acc);
	static void cleanup(vkobjs& mvkobjs,vkvertexbufferdata& vbdata);
};