#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>
namespace renderpass {
	bool init(rvkbucket &rdata);
	void cleanup(rvkbucket &rdata);
};
