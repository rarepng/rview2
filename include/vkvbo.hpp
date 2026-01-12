#pragma once
#include <vulkan/vulkan.h>
#include "core/rvk.hpp"
#include <fastgltf/core.hpp>
class vkvbo {
public:
	static bool init(rvkbucket &mvkobjs, vbodata &vbdata, size_t bsize);
	static bool upload(rvkbucket &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   std::vector<glm::vec3> vertexdata);
	static bool upload(rvkbucket &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   std::vector<glm::vec2> vertexdata);
	static bool upload(rvkbucket &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   const fastgltf::Buffer &buffer, const fastgltf::BufferView &bufferview,
	                   const fastgltf::Accessor &acc);
	static bool upload(rvkbucket &mvkobjs, VkCommandBuffer &cbuffer, vbodata &vbdata,
	                   const std::vector<unsigned int> &jointz);
	static void cleanup(rvkbucket &mvkobjs, vbodata &vbdata);
};
