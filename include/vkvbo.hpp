#pragma once
#include <vulkan/vulkan.h>
#include "core/rvk.hpp"
#include <fastgltf/core.hpp>
namespace vkvbo {
bool init(rvkbucket &mvkobjs, GpuBuffer &vbdata, VkDeviceSize bsize, VkBufferUsageFlags usage);
VkBufferMemoryBarrier2 record_upload(rvkbucket &mvkobjs,
                                     VkCommandBuffer &cbuffer,
                                     GpuBuffer &targetBuffer,
                                     const fastgltf::Buffer &buffer,
                                     const fastgltf::BufferView &bufferview,
                                     const fastgltf::Accessor &acc,
                                     void* mappedStagingStart,
                                     VkBuffer stagingBufferHandle,
                                     VkDeviceSize stagingOffset);
};
