#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>
class renderpass {
public:
	static bool init(rvkbucket &rdata);
	static void cleanup(rvkbucket &rdata);
};
