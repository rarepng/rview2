#pragma once
#include <vulkan/vulkan.h>
//#include <tinygltf/tiny_gltf.h>
#include <fastgltf/core.hpp>
#include "vkobjs.hpp"
class vkebo {
public:
	static bool init(vkobjs& objs, vkebodata& indexbufferdata, size_t buffersize);
	//static bool upload(vkobjs& objs, vkindexbufferdata& indexbufferdata, const tinygltf::Buffer& buffer, const tinygltf::BufferView& bufferview, const tinygltf::Accessor& acc);
	static bool upload(vkobjs& objs,VkCommandBuffer& cbuffer, vkebodata& indexbufferdata, const fastgltf::Buffer& buffer, const fastgltf::BufferView& bufferview, const size_t& count);
	static bool upload(vkobjs& objs, VkCommandBuffer& cbuffer, vkebodata& indexbufferdata,std::vector<unsigned short> indicez);
	static void cleanup(vkobjs& objs, vkebodata& indexbufferdata);
};
