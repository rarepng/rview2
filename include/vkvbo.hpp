#pragma once
#include <vulkan/vulkan.h>
#include "core/rvk.hpp"
#include <fastgltf/core.hpp>
class vkvbo {
public:
	static bool init(rvk &mvkobjs, vbodata &vbdata, size_t bsize);
	static bool upload(rvk &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   std::vector<glm::vec3> vertexdata);
	static bool upload(rvk &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   std::vector<glm::vec2> vertexdata);
	static bool upload(rvk &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   const fastgltf::Buffer &buffer, const fastgltf::BufferView &bufferview,
	                   const fastgltf::Accessor &acc);
	static bool upload(rvk &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   const std::vector<unsigned int> &jointz, const unsigned int count, const unsigned int ofx);
	static void cleanup(rvk &mvkobjs, vbodata &vbdata);
};
