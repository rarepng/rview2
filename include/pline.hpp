#pragma once
#include "core/rvk.hpp"
#include <string>
#include <vulkan/vulkan.h>

namespace pline {
bool init(rvkbucket &mvkobjs, VkPipelineLayout &playout, VkPipeline &pipeline, VkPrimitiveTopology topology,
          std::vector<std::string_view> sfiles, VkFormat cformat = VK_FORMAT_R32G32B32A32_SFLOAT, VkFormat dformat = VK_FORMAT_D32_SFLOAT);
bool initcompute(rvkbucket &mvkobjs, VkPipelineLayout &playout, VkPipeline &pipeline, std::vector<std::string_view> sfiles = {});
void cleanup(rvkbucket &mvkobjs, VkPipeline &pipeline);
};
