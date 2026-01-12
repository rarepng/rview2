#pragma once
#include <vulkan/vulkan.h>
// #include <tinygltf/tiny_gltf.h>
#include "core/rvk.hpp"
#include <fastgltf/core.hpp>
class vkebo {
public:
	static bool init(rvkbucket &objs, ebodata &indexbufferdata, size_t buffersize);
	static bool upload(rvkbucket &objs, VkCommandBuffer &cbuffer, ebodata &indexbufferdata, const fastgltf::Buffer &buffer,
	                   const fastgltf::BufferView &bufferview, const size_t &count,const fastgltf::ComponentType &compType);
	static bool upload(rvkbucket &objs, VkCommandBuffer &cbuffer, ebodata &indexbufferdata,
	                   std::vector<unsigned short> indicez);
	static void cleanup(rvkbucket &objs, ebodata &indexbufferdata);
};
