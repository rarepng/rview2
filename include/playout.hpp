#pragma once
#include "core/rvk.hpp"
#include <vulkan/vulkan.h>

namespace playout {
bool init_bindless(rvkbucket& objs);
void cleanup(rvkbucket &mvkobjs, VkPipelineLayout &vkplayout);
};