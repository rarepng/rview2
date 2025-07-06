#pragma once
#include "core/rvk.hpp"
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
class vksyncobjects {
public:
	static bool init(rvk &rdata);
	static void cleanup(rvk &rdata);
};
