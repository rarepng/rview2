#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>
class renderpass {
public:
	static bool init(rvk &rdata);
	static void cleanup(rvk &rdata);
};
