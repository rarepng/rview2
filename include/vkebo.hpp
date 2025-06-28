#pragma once
#include <vulkan/vulkan.h>
// #include <tinygltf/tiny_gltf.h>
#include "vkobjs.hpp"
#include <fastgltf/core.hpp>
class vkebo {
public:
	static bool init(rvk &objs, ebodata &indexbufferdata, size_t buffersize);
	static bool upload(rvk &objs, VkCommandBuffer &cbuffer, ebodata &indexbufferdata, const fastgltf::Buffer &buffer,
	                   const fastgltf::BufferView &bufferview, const size_t &count);
	static bool upload(rvk &objs, VkCommandBuffer &cbuffer, ebodata &indexbufferdata,
	                   std::vector<unsigned short> indicez);
	static void cleanup(rvk &objs, ebodata &indexbufferdata);
};
