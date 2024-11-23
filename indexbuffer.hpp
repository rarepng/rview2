#pragma once
#include <vulkan/vulkan.h>
#include <tinygltf/tiny_gltf.h>
#include "vkobjs.hpp"
class indexbuffer {
public:
	static bool init(vkobjs& objs, vkindexbufferdata& indexbufferdata, size_t buffersize);
	static bool upload(vkobjs& objs, vkindexbufferdata& indexbufferdata, const tinygltf::Buffer& buffer, const tinygltf::BufferView& bufferview, const tinygltf::Accessor& acc);
	static void cleanup(vkobjs& objs, vkindexbufferdata& indexbufferdata);
};