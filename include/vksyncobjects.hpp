#pragma once
#include "core/rvk.hpp"
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
class vksyncobjects {
public:
	static bool init(rvkbucket &rdata);
	static void cleanup(rvkbucket &rdata);
};
