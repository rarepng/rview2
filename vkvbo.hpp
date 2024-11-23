#pragma once
#include <vulkan/vulkan.h>
//#include <tinygltf/tiny_gltf.h>
#include <fastgltf/core.hpp>
#include "vkobjs.hpp"
class vkvbo {
public:
	static bool init(vkobjs& mvkobjs,vkvertexbufferdata& vbdata, size_t bsize);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, vkmesh vmesh);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, std::vector<glm::vec3> vertexdata);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, std::vector<glm::vec2> vertexdata);
	//static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, const tinygltf::Buffer& buffer, const tinygltf::BufferView& bufferview, const tinygltf::Accessor& acc);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, const fastgltf::Buffer& buffer, const fastgltf::BufferView& bufferview, const fastgltf::Accessor& acc);
	static bool upload(vkobjs& mvkobjs, vkvertexbufferdata& vbdata, const std::vector<unsigned int>& jointz,const unsigned int count,const unsigned int ofx);
	static void cleanup(vkobjs& mvkobjs,vkvertexbufferdata& vbdata);
};
