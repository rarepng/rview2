#include "backends/imgui_impl_sdl3.h"
#include "core/rvk.hpp"
#include <cstdint>
#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION

// #define _DEBUG

#include "exp/particle.hpp"
#include "vkrenderer.hpp"

// #include "Jolt/Physics/SoftBody/SoftBodyCreationSettings.h"
// #include "Jolt/Physics/SoftBody/SoftBodySharedSettings.h"

#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <vk_mem_alloc.h>

#include <cstdlib>
#include <ctime>
#include <future>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/spline.hpp>
#include <iostream>
#include <ranges>

#include "vk/commandbuffer.hpp"
#include "vk/commandpool.hpp"
#include "vksyncobjects.hpp"
// #ifdef _DEBUG
// #include "logger.hpp"
// #endif
#include <SDL3/SDL_vulkan.h>
// #include <unistd.h>

#include "exp/particle.hpp"

#include "vktex.hpp"
#include "playout.hpp"
#include "ubo.hpp"

// yep revereted all modules


void vkrenderer::immediate_submit(rvkbucket& mvkobjs,
                                  std::function<void(VkCommandBuffer cbuffer)> &&fn) {
	VkCommandBufferAllocateInfo allocInfo{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = mvkobjs.cpools_graphics.at(0);
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cbuffer;
	vkAllocateCommandBuffers(mvkobjs.vkdevice.device, &allocInfo, &cbuffer);

	VkCommandBufferBeginInfo beginInfo{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cbuffer, &beginInfo);

	fn(cbuffer);

	vkEndCommandBuffer(cbuffer);

	VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cbuffer;

	VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	VkFence tempFence;
	vkCreateFence(mvkobjs.vkdevice.device, &fenceInfo, nullptr, &tempFence);

	mvkobjs.mtx2->lock();
	vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitInfo, tempFence);
	mvkobjs.mtx2->unlock();

	vkWaitForFences(mvkobjs.vkdevice.device, 1, &tempFence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(mvkobjs.vkdevice.device, tempFence, nullptr);

	vkFreeCommandBuffers(mvkobjs.vkdevice.device, mvkobjs.cpools_graphics.at(0),
	                     1, &cbuffer);
}
std::function<void(rvkbucket::DummyTexture&,rvkbucket& mvkobjs)> destroyDummy =
[](rvkbucket::DummyTexture& tex,rvkbucket& mvkobjs) {
	vkDestroyImageView(mvkobjs.vkdevice.device, tex.view, nullptr);
	vkDestroyImage(mvkobjs.vkdevice.device, tex.image, nullptr);
	vkFreeMemory(mvkobjs.vkdevice.device, tex.memory, nullptr);
};
// temp
void UpdateAllInstances(std::vector<genericinstance *> &instances) {
	std::ranges::for_each(instances,
	[](genericinstance *inst) {
		inst->checkforupdates();
	});
}

// gotta move these somewhere more reasonable someday
bool init_global_samplers(rvkbucket &objs) {
	VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	info.magFilter = VK_FILTER_LINEAR;
	info.minFilter = VK_FILTER_LINEAR;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.minLod = 0.0f;
	info.maxLod = VK_LOD_CLAMP_NONE;
	info.maxAnisotropy = 16.0f;
	info.anisotropyEnable = VK_TRUE;

	if (vkCreateSampler(objs.vkdevice.device, &info, nullptr,
	                    &objs.samplerz[0]) != VK_SUCCESS) {
		return false;
	}

	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vkCreateSampler(objs.vkdevice.device, &info, nullptr, &objs.samplerz[1]);

	return true;
}

bool init_dummy_textures(rvkbucket &objs, VkCommandPool &cmdPool) {

	auto find_memory_type = [&](uint32_t typeFilter,
	VkMemoryPropertyFlags properties) -> uint32_t {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(objs.vkdevice.physical_device,
		                                    &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) &&
			        (memProperties.memoryTypes[i].propertyFlags & properties) ==
			        properties) {
				return i;
			}
		}
		return 0;
	};
	auto create_buffer = [&](VkDeviceSize size, VkBufferUsageFlags usage,
	                         VkMemoryPropertyFlags properties, VkBuffer &buffer,
	VkDeviceMemory &bufferMemory) {
		VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		vkCreateBuffer(objs.vkdevice.device, &bufferInfo, nullptr, &buffer);

		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(objs.vkdevice.device, buffer, &memReqs);

		VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex =
		    find_memory_type(memReqs.memoryTypeBits, properties);

		vkAllocateMemory(objs.vkdevice.device, &allocInfo, nullptr, &bufferMemory);
		vkBindBufferMemory(objs.vkdevice.device, buffer, bufferMemory, 0);
	};

	auto record_barrier = [](VkCommandBuffer cmd, VkImage image,
	                         VkImageLayout oldLayout, VkImageLayout newLayout,
	                         VkAccessFlags srcAccess, VkAccessFlags dstAccess,
	                         VkPipelineStageFlags srcStage,
	VkPipelineStageFlags dstStage) {
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = srcAccess;
		barrier.dstAccessMask = dstAccess;

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
		                     &barrier);
	};

	auto create_1x1_texture = [&](uint32_t pixelData) -> rvkbucket::DummyTexture {
		rvkbucket::DummyTexture tex{};

		VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = {1, 1, 1};
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage =
		    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		vkCreateImage(objs.vkdevice.device, &imageInfo, nullptr, &tex.image);

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(objs.vkdevice.device, tex.image, &memReqs);

		VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type(
		                                memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		vkAllocateMemory(objs.vkdevice.device, &allocInfo, nullptr, &tex.memory);
		vkBindImageMemory(objs.vkdevice.device, tex.image, tex.memory, 0);

		VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		viewInfo.image = tex.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		vkCreateImageView(objs.vkdevice.device, &viewInfo, nullptr, &tex.view);

		VkBuffer stagingBuf;
		VkDeviceMemory stagingMem;
		VkDeviceSize imageSize = 4;

		create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		              stagingBuf, stagingMem);

		// Map & Copy Data
		void *data;
		vkMapMemory(objs.vkdevice.device, stagingMem, 0, imageSize, 0, &data);
		memcpy(data, &pixelData, (size_t)imageSize);
		vkUnmapMemory(objs.vkdevice.device, stagingMem);

		vkrenderer::immediate_submit(objs,[&](VkCommandBuffer cmd) {
			record_barrier(
			    cmd, tex.image, VK_IMAGE_LAYOUT_UNDEFINED,
			    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
			    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			region.imageOffset = {0, 0, 0};
			region.imageExtent = {1, 1, 1};

			vkCmdCopyBufferToImage(cmd, stagingBuf, tex.image,
			                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			record_barrier(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			               VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			               VK_PIPELINE_STAGE_TRANSFER_BIT,
			               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		});

		vkDestroyBuffer(objs.vkdevice.device, stagingBuf, nullptr);
		vkFreeMemory(objs.vkdevice.device, stagingMem, nullptr);

		return tex;
	};

	objs.defaults.purple = create_1x1_texture(0xDD00DDDD); // error texture
	objs.defaults.white = create_1x1_texture(0xFFFFFFFF);
	objs.defaults.normal = create_1x1_texture(0xFFFF8080);
	objs.defaults.black = create_1x1_texture(0xFF000000);

	std::array<VkDescriptorImageInfo, 4> dummyInfos{};
	std::array<VkWriteDescriptorSet, 4> dummyWrites{};

	dummyInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[0].imageView = objs.defaults.purple.view;
	dummyInfos[0].sampler = objs.samplerz[0];

	dummyInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[1].imageView = objs.defaults.white.view;
	dummyInfos[1].sampler = objs.samplerz[0];

	dummyInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[2].imageView = objs.defaults.normal.view;
	dummyInfos[2].sampler = objs.samplerz[0];

	dummyInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[3].imageView = objs.defaults.black.view;
	dummyInfos[3].sampler = objs.samplerz[0];

	for (uint32_t i = 0; i < 4; ++i) {
		dummyWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		dummyWrites[i].dstSet = rvkbucket::globalBindlessSet;
		dummyWrites[i].dstBinding = 0;
		dummyWrites[i].dstArrayElement = i;
		dummyWrites[i].descriptorCount = 1;
		dummyWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		dummyWrites[i].pImageInfo = &dummyInfos[i];
	}

	vkUpdateDescriptorSets(objs.vkdevice.device, 4, dummyWrites.data(), 0, nullptr);

	return true;
}
float map2(glm::vec3 x) {
	return std::max(x.y, 0.0f);
}

bool vkrenderer::init(rvkbucket& mvkobjs) {
	std::srand(static_cast<int>(time(NULL)));
	if (!mvkobjs.wind)
		return false;
	// if (!std::apply(
	// [](auto &&...f) {
	// return (... && f());
	// }, std::tuple{[&mvkobjs]() {
	// 	return initvma(mvkobjs) && getqueue(mvkobjs) &&
	// 	       createswapchain(mvkobjs) && createdepthbuffer(mvkobjs) &&
	// 	       createcommandpool(mvkobjs) && createcommandbuffer(mvkobjs) &&
	// 	       createsyncobjects(mvkobjs) && createpools(mvkobjs) && initcpuQs(mvkobjs) &&
	// 		   playout::init_bindless(mvkobjs) &&
	// 		   ubo::init_global(mvkobjs) &&
	// 		   initglobalmats(mvkobjs) &&
	// 	       init_global_samplers(mvkobjs) &&
	// 	       init_dummy_textures(mvkobjs,
	// 	                           mvkobjs.cpools_graphics.at(0)) &&
	// 	       rview::rvk::tex::load_env_map(mvkobjs, mvkobjs.exrtex.at(0));
	// 			//  ubo::init_global(mvkobjs);
	// 	// might switch to passing mvk idk, maybe replace the whole mvkobjs
	// 	// bucket with something more functional
	// }}))
	// 	return false;

	if (!initvma(mvkobjs)) return false;
    if (!getqueue(mvkobjs)) return false;
    if (!createswapchain(mvkobjs)) return false;
    if (!createdepthbuffer(mvkobjs)) return false;
    if (!createcommandpool(mvkobjs)) return false;
    if (!createcommandbuffer(mvkobjs)) return false;
    if (!createsyncobjects(mvkobjs)) return false;
    if (!initcpuQs(mvkobjs)) return false;

    if (!playout::init_bindless(mvkobjs)) return false;
	if (!init_global_samplers(mvkobjs)) return false;
	if (!init_dummy_textures(mvkobjs, mvkobjs.cpools_graphics.at(0))) return false;
	if (!rview::rvk::tex::load_env_map(mvkobjs, mvkobjs.exrtex.at(0))) return false;
    
    vkDeviceWaitIdle(mvkobjs.vkdevice.device); 

    if (!ubo::init_global(mvkobjs)) return false;
    if (!initglobalmats(mvkobjs)) return false;

	if (!initglobalinstances(mvkobjs))return false;

	if (!initglobalindirect(mvkobjs))return false;

    vkDeviceWaitIdle(mvkobjs.vkdevice.device);




	mvkobjs.height = mvkobjs.schain.extent.height;
	mvkobjs.width = mvkobjs.schain.extent.width;

	if (initui(mvkobjs)) {
		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	} else
		return false;

	mframetimer.start();

	return true;
}
bool vkrenderer::initcpuQs(rvkbucket& mvkobjs) {
	return mvkobjs.sbelt.init(mvkobjs.alloc, 256 * 1024 * 1024);
}
bool vkrenderer::initscene(rvkbucket& mvkobjs) {
	mplayer.reserve(playerfname.size());
	mplayer.resize(playerfname.size());

	selectiondata.instancesettings.reserve(playercount.size());
	selectiondata.instancesettings.resize(playercount.size());

	size_t idx{0};

	for (auto &i : mplayer) {
		selectiondata.instancesettings.at(idx).reserve(playercount[idx]);
		selectiondata.instancesettings.at(idx).resize(playercount[idx]);
		i = std::make_shared<playoutgeneric>();
		if (!i->setup(mvkobjs, playerfname[idx], playercount[idx],
		              playershaders[idx][0], playershaders[idx][1]))
			return false;
		for (size_t j{0}; j < playercount[idx]; j++)
			selectiondata.instancesettings.at(idx).at(j) =
			                                  &i->getinst(j)->getinstancesettings();
		idx++;
	}
	playerfname.clear();
	playerfname.shrink_to_fit();
	playercount.clear();
	playercount.shrink_to_fit();

	if (!particle::createeverything(mvkobjs))
		return false;

	return true;
}
bool vkrenderer::initglobalmats(rvkbucket &mvkobjs){
	size_t bufferSize = rvkbucket::MAX_GLOBAL_MATERIALS * sizeof(MaterialData);
	VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	binfo.size = bufferSize;
	binfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VmaAllocationCreateInfo vmaallocinfo{};
	vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	vmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocResult;
	if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo, 
		&rvkbucket::global_materials.buffer, 
		&rvkbucket::global_materials.alloc, &allocResult) != VK_SUCCESS) {
		return false;
	}
	rvkbucket::global_materials.mapped_data = allocResult.pMappedData;

	for (uint32_t i = 0; i < rvkbucket::MAX_GLOBAL_MATERIALS; ++i) {
		rvkbucket::global_materials.free_slots.push(i);
	}

	VkDescriptorBufferInfo ssboInfo{ rvkbucket::global_materials.buffer, 0, bufferSize };
	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstSet = rvkbucket::globalBindlessSet;
	write.dstBinding = 4;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	write.pBufferInfo = &ssboInfo;

	vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &write, 0, nullptr);
	return true;
}
bool vkrenderer::initglobalinstances(rvkbucket& mvkobjs){
	mvkobjs.frameInstances.reserve(SceneData::MAX_ENTITIES);
	for (uint32_t i = 0; i < rvkbucket::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkBufferCreateInfo ibinfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		ibinfo.size = sizeof(GPUInstanceData) * SceneData::MAX_ENTITIES;
		ibinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		VmaAllocationCreateInfo ivmaallocinfo{};
		ivmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		ivmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (vmaCreateBuffer(mvkobjs.alloc, &ibinfo, &ivmaallocinfo,
		                    &mvkobjs.globalInstanceBuffers[i].buffer,
		                    &mvkobjs.globalInstanceBuffers[i].alloc, nullptr) != VK_SUCCESS) {
			return false;
		}

		VkDescriptorBufferInfo issboInfo{mvkobjs.globalInstanceBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet iwrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		iwrite.dstSet = rvkbucket::globalBindlessSet;
		iwrite.dstBinding = 7;
		iwrite.dstArrayElement = i;
		iwrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		iwrite.descriptorCount = 1;
		iwrite.pBufferInfo = &issboInfo;

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &iwrite, 0, nullptr);
	}
	return true;
}
bool vkrenderer::initglobalindirect(rvkbucket& mvkobjs){
	for (uint32_t i = 0; i < rvkbucket::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkBufferCreateInfo ibinfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		ibinfo.size = sizeof(VkDrawIndexedIndirectCommand) * SceneData::MAX_ENTITIES;
		ibinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

		VmaAllocationCreateInfo ivmaallocinfo{};
		ivmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		ivmaallocinfo.flags = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // VMA_MEMORY_USAGE_GPU_ONLY?

		if (vmaCreateBuffer(mvkobjs.alloc, &ibinfo, &ivmaallocinfo,
		                    &mvkobjs.globalIndirectBuffers[i].buffer,
		                    &mvkobjs.globalIndirectBuffers[i].alloc, nullptr) != VK_SUCCESS) {
			return false;
		}

		VkDescriptorBufferInfo issboInfo{mvkobjs.globalIndirectBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet iwrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		iwrite.dstSet = rvkbucket::globalBindlessSet;
		iwrite.dstBinding = 8;
		iwrite.dstArrayElement = i;
		iwrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		iwrite.descriptorCount = 1;
		iwrite.pBufferInfo = &issboInfo;

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &iwrite, 0, nullptr);
	}
	return true;
}
bool vkrenderer::initvma(rvkbucket& mvkobjs) {
	VmaAllocatorCreateInfo allocinfo{};
	allocinfo.vulkanApiVersion = VK_API_VERSION_1_4;
	allocinfo.physicalDevice = mvkobjs.vkdevice.physical_device;
	allocinfo.device = mvkobjs.vkdevice.device;
	allocinfo.instance = mvkobjs.inst.instance;
	VmaVulkanFunctions vma_functions{};
	vma_functions.vkGetInstanceProcAddr = mvkobjs.inst.fp_vkGetInstanceProcAddr;
	vma_functions.vkGetDeviceProcAddr = mvkobjs.inst.fp_vkGetDeviceProcAddr;
	allocinfo.pVulkanFunctions = &vma_functions;
	//dbg only require VK_EXT_MEMORY_BUDGET_EXTENSION_NAME
	// allocinfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	if (vmaCreateAllocator(&allocinfo, &mvkobjs.alloc) != VK_SUCCESS) {
		return false;
	}
	return true;
}
bool vkrenderer::getqueue(rvkbucket& mvkobjs) {
	auto graphqueueret = mvkobjs.vkdevice.get_queue(vkb::QueueType::graphics);
	mvkobjs.graphicsQ = graphqueueret.value();
	auto presentqueueret = mvkobjs.vkdevice.get_queue(vkb::QueueType::present);
	mvkobjs.presentQ = presentqueueret.value();
	auto computequeueret = mvkobjs.vkdevice.get_queue(vkb::QueueType::compute);
	mvkobjs.computeQ = computequeueret.value();
	auto transqueueret = mvkobjs.vkdevice.get_queue(vkb::QueueType::transfer);
	mvkobjs.transferQ = transqueueret.value();

	return true;
}
bool vkrenderer::createdepthbuffer(rvkbucket& mvkobjs) {
	VkExtent3D depthimageextent = {mvkobjs.schain.extent.width,
	                               mvkobjs.schain.extent.height, 1
	                              };

	mvkobjs.rddepthformat = VK_FORMAT_D32_SFLOAT;

	VkImageCreateInfo depthimginfo{};
	depthimginfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthimginfo.imageType = VK_IMAGE_TYPE_2D;
	depthimginfo.format = mvkobjs.rddepthformat;
	depthimginfo.extent = depthimageextent;
	depthimginfo.mipLevels = 1;
	depthimginfo.arrayLayers = 1;
	depthimginfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthimginfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthimginfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VmaAllocationCreateInfo depthallocinfo{};
	depthallocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	depthallocinfo.requiredFlags =
	    VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vmaCreateImage(mvkobjs.alloc, &depthimginfo, &depthallocinfo,
	                   &mvkobjs.rddepthimage, &mvkobjs.rddepthimagealloc,
	                   nullptr) != VK_SUCCESS) {
		return false;
	}

	VkImageViewCreateInfo depthimgviewinfo{};
	depthimgviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthimgviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthimgviewinfo.image = mvkobjs.rddepthimage;
	depthimgviewinfo.format = mvkobjs.rddepthformat;
	depthimgviewinfo.subresourceRange.baseMipLevel = 0;
	depthimgviewinfo.subresourceRange.levelCount = 1;
	depthimgviewinfo.subresourceRange.baseArrayLayer = 0;
	depthimgviewinfo.subresourceRange.layerCount = 1;
	depthimgviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	if (vkCreateImageView(mvkobjs.vkdevice, &depthimgviewinfo, nullptr,
	                      &mvkobjs.rddepthimageview) != VK_SUCCESS) {
		return false;
	}

	return true;
}
bool vkrenderer::createswapchain(rvkbucket& mvkobjs) {
	vkb::SwapchainBuilder swapchainbuild{mvkobjs.vkdevice};

	VkSurfaceCapabilitiesKHR surcap;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.vkdevice.physical_device,
	        mvkobjs.surface, &surcap);

	swapchainbuild.set_composite_alpha_flags((
	            VkCompositeAlphaFlagBitsKHR)(surcap.supportedCompositeAlpha & 8 ? 8 : 1));
	swapchainbuild.set_desired_format(
	{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
	auto swapchainbuilret =
	    swapchainbuild.set_old_swapchain(mvkobjs.schain)
	    .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
	    .build();
	if (!swapchainbuilret) {
		return false;
	}
	vkb::destroy_swapchain(mvkobjs.schain);
	mvkobjs.schain = swapchainbuilret.value();
	mvkobjs.schainimgs = mvkobjs.schain.get_images().value();
	mvkobjs.schainimgviews = mvkobjs.schain.get_image_views().value();
	return true;
}
bool vkrenderer::recreateswapchain(rvkbucket& mvkobjs) {
	SDL_GetWindowSize(mvkobjs.wind, &mvkobjs.width, &mvkobjs.height);

	if (mvkobjs.width == 0 || mvkobjs.height == 0) {
		return true;
	}

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);
	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview,
	                   nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage,
	                mvkobjs.rddepthimagealloc);

	mvkobjs.schain.destroy_image_views(mvkobjs.schainimgviews);

	if (!createswapchain(mvkobjs)) {
		return false;
	}
	if (!createdepthbuffer(mvkobjs)) {
		return false;
	}
	return true;
}
bool vkrenderer::createcommandpool(rvkbucket& mvkobjs) {
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_graphics,vkb::QueueType::graphics)) [[unlikely]] {
		return false;
	}
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_compute,vkb::QueueType::compute)) [[unlikely]] {
		return false;
	}
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_transfer,vkb::QueueType::transfer)) [[unlikely]] {
		return false;
	}
	return true;
}
bool vkrenderer::createcommandbuffer(rvkbucket& mvkobjs) {
	for (auto [cpool, cbuffers] : std::views::zip(mvkobjs.cpools_graphics, mvkobjs.cbuffers_graphics)) {
		if (!commandbuffer::create(mvkobjs, cpool, cbuffers)) [[unlikely]] {
			return false;
		}
	}
	for (auto [cpool, cbuffers] : std::views::zip(mvkobjs.cpools_compute, mvkobjs.cbuffers_compute)) {
		if (!commandbuffer::create(mvkobjs, cpool, cbuffers)) [[unlikely]] {
			return false;
		}
	}
	for (auto [cpool, cbuffers] : std::views::zip(mvkobjs.cpools_transfer, mvkobjs.cbuffers_transfer)) {
		if (!commandbuffer::create(mvkobjs, cpool, cbuffers)) [[unlikely]] {
			return false;
		}
	}
	return true;
}
bool vkrenderer::createsyncobjects(rvkbucket& mvkobjs) {
	if (!vksyncobjects::init(mvkobjs))
		return false;
	return true;
}
bool vkrenderer::initui(rvkbucket& mvkobjs) {
	if (!ui::init(mvkobjs))
		return false;
	return true;
}
void vkrenderer::cleanup(rvkbucket& mvkobjs) {

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);

	particle::destroyeveryting(mvkobjs);

	ui::cleanup(mvkobjs);

	vksyncobjects::cleanup(mvkobjs);
	for (auto &x : mvkobjs.dpools)
		rpool::destroy(mvkobjs.vkdevice.device, x);

	for (const auto &i : mplayer)
		i->cleanuplines(mvkobjs);

	for (const auto &i : mplayer)
		i->cleanupbuffers(mvkobjs);

	for (const auto &i : mplayer)
		i->cleanupmodels(mvkobjs);

	//temp
	mvkobjs.cleanupQ.flush();
	mvkobjs.deletionQ.flush();

	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview,
	                   nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage,
	                mvkobjs.rddepthimagealloc);

	if (mvkobjs.exrtex.at(0).img) {
		vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.exrtex.at(0).imgview,
		                   nullptr);
		vkDestroySampler(mvkobjs.vkdevice.device, mvkobjs.exrtex.at(0).imgsampler,
		                 nullptr);
		vmaDestroyImage(mvkobjs.alloc, mvkobjs.exrtex.at(0).img,
		                mvkobjs.exrtex.at(0).alloc);
	}

	mvkobjs.sbelt.free(mvkobjs.alloc);

	vmaDestroyBuffer(mvkobjs.alloc, rvkbucket::global_materials.buffer, rvkbucket::global_materials.alloc);

	vkDestroyDescriptorPool(mvkobjs.vkdevice.device, rvkbucket::global_materials.dedicated_pool, VK_NULL_HANDLE);

	vkDestroyPipelineLayout(mvkobjs.vkdevice.device, rvkbucket::globalPipelineLayout, nullptr);
	vkDestroyDescriptorPool(mvkobjs.vkdevice.device, rvkbucket::globalBindlessPool, nullptr);
	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, rvkbucket::globalBindlessLayout, nullptr);
	vmaDestroyBuffer(mvkobjs.alloc, rvkbucket::globalCameraUBO.buffer, rvkbucket::globalCameraUBO.alloc);
	for(auto&& x: mvkobjs.globalInstanceBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);
	for(auto&& x: mvkobjs.globalIndirectBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	//dbg only block
	// char* statsString = nullptr;
	// vmaBuildStatsString(mvkobjs.alloc, &statsString, true);
	// if (statsString) {
	// 	std::cout << "=== VMA REPORT !! ===" << std::endl;
	// 	std::cout << statsString << std::endl;
	// 	std::cout << "=======================" << std::endl;
	// 	vmaFreeStatsString(mvkobjs.alloc, statsString);
	// }

	vmaDestroyAllocator(mvkobjs.alloc);

	destroyDummy(mvkobjs.defaults.purple,mvkobjs);
	destroyDummy(mvkobjs.defaults.white,mvkobjs);
	destroyDummy(mvkobjs.defaults.normal,mvkobjs);
	destroyDummy(mvkobjs.defaults.black,mvkobjs);

	for (const auto &i : mvkobjs.samplerz)
		vkDestroySampler(mvkobjs.vkdevice, i, nullptr);

	mvkobjs.schain.destroy_image_views(mvkobjs.schainimgviews);
	vkb::destroy_swapchain(mvkobjs.schain);

	vkb::destroy_device(mvkobjs.vkdevice);
	vkb::destroy_surface(mvkobjs.inst.instance, mvkobjs.surface);
	vkb::destroy_instance(mvkobjs.inst);
}

void vkrenderer::setsize(rvkbucket& mvkobjs,unsigned int w, unsigned int h) {
	mvkobjs.width = w;
	mvkobjs.height = h;
	if (!w || !h)
		persviewproj.at(1) = glm::perspective(
		                         glm::radians(static_cast<float>(mvkobjs.schain.extent.width)),
		                         static_cast<float>(mvkobjs.schain.extent.height) /
		                         static_cast<float>(mvkobjs.height),
		                         1.0f, 2.0f);
	else
		persviewproj.at(1) = glm::perspective(
		                         mvkobjs.fov,
		                         static_cast<float>(mvkobjs.width) / static_cast<float>(mvkobjs.height),
		                         0.002f, 2000.0f);
}

bool vkrenderer::uploadfordraw(rvkbucket& mvkobjs,VkCommandBuffer cbuffer) {
	manimupdatetimer.start();

	for (const auto &i : mplayer)
		i->uploadvboebo(mvkobjs, cbuffer);
	return true;
}

bool vkrenderer::uploadfordraw(rvkbucket& mvkobjs,std::shared_ptr<playoutgeneric> &x) {
	ZoneScoped;
	manimupdatetimer.start();

	x->uploadvboebo(mvkobjs,
	                mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame));

	mvkobjs.uploadubossbotime = manimupdatetimer.stop();

	return true;
}

void vkrenderer::sdlevent(rvkbucket& mvkobjs,const SDL_Event& e) {
	switch (e.type) {
	case SDL_EVENT_KEY_DOWN:
		[[likely]]
	case SDL_EVENT_KEY_UP:
		[[likely]]
		{
			bool is_down = (e.type == SDL_EVENT_KEY_DOWN);
			switch (e.key.key) {
			case SDLK_W:
				input_state[Key_W] = is_down;
				break;
			case SDLK_S:
				input_state[Key_S] = is_down;
				break;
			case SDLK_A:
				input_state[Key_A] = is_down;
				break;
			case SDLK_D:
				input_state[Key_D] = is_down;
				break;
			case SDLK_Q:
				input_state[Key_Q] = is_down;
				break;
			case SDLK_E:
				input_state[Key_E] = is_down;
				break;

			case SDLK_F4:
				if (is_down) {
					mvkobjs.fullscreen = !mvkobjs.fullscreen;
					SDL_SetWindowFullscreen(mvkobjs.wind, mvkobjs.fullscreen);
				}
				break;
			case SDLK_ESCAPE:
				if (is_down) mvkobjs.mshutdown = true;
				break;
			}
			break;
		}

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (e.button.button == SDL_BUTTON_RIGHT) input_state[Key_RightClick] = true;
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		if (e.button.button == SDL_BUTTON_RIGHT) input_state[Key_RightClick] = false;
		break;
	case SDL_EVENT_WINDOW_MINIMIZED:
		[[unlikely]]
		{
			SDL_Event min_event;
			while (SDL_WaitEvent(&min_event)) {
				if (min_event.type == SDL_EVENT_WINDOW_RESTORED) break;
				if (min_event.type == SDL_EVENT_QUIT) {
					mvkobjs.mshutdown = true;
					break;
				}
			}
			break;
		}

	case SDL_EVENT_DROP_FILE:
    [[unlikely]]
    {
        std::string fname = e.drop.data;
        
        g_jobs.enqueue([&mvkobjs, fname]() {
            auto newp = std::make_shared<playoutgeneric>();

            bool success = newp->setup(mvkobjs, fname.c_str(), 1,
                                       playershaders[0][0], playershaders[0][1]);

            if (success) {
                if (!pending_models.push(std::move(newp))) {
                    std::cout << "Engine queue full! Dropped model: " << fname << "\n";
                }
            } else {
                std::cout << "model not added, probably wrong format. \n"
                          << "only binary gltf files (.glb) are accepted. Provided was: " 
                          << fname << std::endl;
            }
        });
        break;
    }
	case SDL_EVENT_DROP_COMPLETE:
		break;
	}
}
void vkrenderer::moveplayer() {
	//this was to add delay to the movement as in gradual movement as in movement in games
	// currently selected
	// modelsettings &s = mplayer[selectiondata.midx]
	//                    ->getinst(selectiondata.iidx)
	//                    ->getinstancesettings();
	// s.msworldpos = playermoveto;
}

void vkrenderer::movecam(rvkbucket& mvkobjs) {
	static ImGuiIO* io = &ImGui::GetIO();
	float mx, my;
	const SDL_MouseButtonFlags mouseMask = SDL_GetMouseState(&mx, &my);
	const bool isRightClick = (mouseMask & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT));
	const bool isLeftClick  = (mouseMask & SDL_BUTTON_MASK(SDL_BUTTON_LEFT));
	float speed = 2.0f;
	mvkobjs.camfor   = (float(input_state[Key_W]) * speed) - (float(input_state[Key_S]) * speed);
	mvkobjs.camright = (float(input_state[Key_D]) * speed) - (float(input_state[Key_A]) * speed);
	mvkobjs.camup    = (float(input_state[Key_E]) * speed) - (float(input_state[Key_Q]) * speed);

	static bool wasLooking = false;
	auto* win = reinterpret_cast<SDL_Window*>(mvkobjs.wind);

	if (isRightClick != wasLooking) {
		if (isRightClick) {
			SDL_SetWindowRelativeMouseMode(win, true);

			float ignoreX, ignoreY;
			SDL_GetRelativeMouseState(&ignoreX, &ignoreY);
		}
		else {
			SDL_SetWindowRelativeMouseMode(win, false);

			SDL_WarpMouseInWindow(win, static_cast<float>(mousex), static_cast<float>(mousey));
		}
		wasLooking = isRightClick;
	}

	if (isRightClick) {
		float rx, ry;
		SDL_GetRelativeMouseState(&rx, &ry);
		mvkobjs.azimuth   += rx * 0.1f;
		mvkobjs.elevation -= ry * 0.1f;
		mvkobjs.elevation = std::clamp(mvkobjs.elevation, -89.0f, 89.0f);
	}
	else {
		mousex = static_cast<int>(mx);
		mousey = static_cast<int>(my);
	}
	persviewproj.at(0) = mcam.getview(mvkobjs);

	if (!io->WantCaptureMouse && isLeftClick && !isRightClick) {
		glm::vec4 viewport(0.0f, 0.0f, (float)mvkobjs.width, (float)mvkobjs.height);

		glm::vec3 nearPt = glm::unProject(glm::vec3(mx, viewport.w - my, 0.0f),
		                                  persviewproj.at(0), persviewproj.at(1), viewport);
		glm::vec3 farPt  = glm::unProject(glm::vec3(mx, viewport.w - my, 1.0f),
		                                  persviewproj.at(0), persviewproj.at(1), viewport);

		glm::vec3 d = glm::normalize(farPt - nearPt);

		if (glm::abs(d.y) > 0.01f) {
			float t = (0.0f - nearPt.y) / d.y;
			if (t >= 0.0f) {
				glm::vec3 h = nearPt + t * d;
				h.y = navmesh(h.x, h.z);

				modelsettings &s = mplayer[selectiondata.midx]
				                   ->getinst(selectiondata.iidx)
				                   ->getinstancesettings();
				s.msworldpos = h;
				playerlookto = glm::normalize(h - s.msworldpos);
				if (glm::abs(h.x - s.msworldpos.x) > 2.1f || glm::abs(h.z - s.msworldpos.z) > 2.1f) {
					s.msworldrot.y = glm::degrees(glm::atan(playerlookto.x, playerlookto.z));
				}
			}
		}
	}
}

float vkrenderer::navmesh(float x, float z) {
	return 0.0f;
}

glm::vec3 vkrenderer::navmeshnormal(float x, float z) {
	return glm::vec3{0.0f, 1.0f, 0.0f};
}


bool vkrenderer::draw(rvkbucket& mvkobjs) {
	ZoneScoped;

	if (vkWaitForFences(mvkobjs.vkdevice.device, 1,
	                    &mvkobjs.fencez.at(rvkbucket::currentFrame), VK_TRUE,
	                    UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.vkdevice.device, 1,
	                  &mvkobjs.fencez.at(rvkbucket::currentFrame)) != VK_SUCCESS)
		return false;

	//temp
	mvkobjs.deletionQ.flush();

	auto& currentgraph = mvkobjs.frameGraphs[rvkbucket::currentFrame];
	currentgraph.clear();


	// mvkobjs.frameInstances.clear();


	VkCommandBuffer c = mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame);

	

	if (vkResetCommandBuffer(c, 0) != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(
	            c, &cmdbgninfo) !=
	        VK_SUCCESS)
		return false;



	double tick = static_cast<double>(SDL_GetTicks()) / 1000.0;
	mvkobjs.tickdiff = tick - mlasttick;
	mvkobjs.frametime = mframetimer.stop();
	mframetimer.start();

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(
	                   mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                   mvkobjs.semaphorez[0][rvkbucket::currentFrame], VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreateswapchain(mvkobjs);
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}


	if (mvkobjs.schain.extent.width == 0 || mvkobjs.schain.extent.height == 0)
		return true;


	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		ImGui_ImplSDL3_ProcessEvent(&e);
		sdlevent(mvkobjs,e);
	}

	movecam(mvkobjs);

	
	// isdfk
	std::shared_ptr<playoutgeneric> newly_loaded_model;

	while (pending_models.pop(newly_loaded_model)) {
		uploadfordraw(mvkobjs, newly_loaded_model);
		mplayer.push_back(newly_loaded_model);
		
		selectiondata.instancesettings.emplace_back();
		selectiondata.instancesettings.back().emplace_back(
			&mplayer.back()->getinst(0)->getinstancesettings()
		);
	}
	// joint anims
	// if (dummytick / 2) {
	for (const auto &i : mplayer)
		i->updateanims();
	// 	dummytick = 0;
	// }

	mmatupdatetimer.start();

	// joint mats
	for (const auto &i : mplayer)
		i->updatemats();

	mvkobjs.updatemattime = mmatupdatetimer.stop();



	// for (const auto &player : mplayer) {
    //     uint32_t modelID = player->get_modelID();
    //     bool isSkinned = player->is_skinned();
    //     uint32_t stride = isSkinned ? player->getinst(0)->getjointmatrixsize() : 1;

    //     for (size_t i = 0; i < player->instcount(); ++i) {
    //         auto inst = player->getinst(i);
    //         if (!inst->getinstancesettings().msdrawmodel) continue;

    //         GPUInstanceData idata{};
    //         idata.worldTransform = inst->calcstaticmat();
    //         idata.modelID = modelID;
    //         idata.jointOffset = i * stride;
    //         idata.isSkinned = isSkinned ? 1 : 0;
    //         idata.indexCount = player->getindexcount(); 
    //         idata.firstIndex = player->getfirstindex();
    //         idata.vertexOffset = player->getvertexoffset();
    //         idata.padding = 0;

    //         mvkobjs.frameInstances.push_back(idata);
    //     }
    // }

	// if (!mvkobjs.frameInstances.empty()) {
    //     void* data;
    //     vmaMapMemory(mvkobjs.alloc, mvkobjs.globalInstanceBuffers[rvkbucket::currentFrame].alloc, &data);
    //     std::memcpy(data, mvkobjs.frameInstances.data(), mvkobjs.frameInstances.size() * sizeof(GPUInstanceData));
    //     vmaUnmapMemory(mvkobjs.alloc, mvkobjs.globalInstanceBuffers[rvkbucket::currentFrame].alloc);
    // }


	// joint check
	// if (dummytick % 2) {
	for (const auto &i : mplayer)
		for(size_t j{0};j<i->instcount();j++)
			i->getinst(j)->checkforupdates();
	// }

	// dummytick++;

	moveplayer();

	
	muploadubossbotimer.start();

	{
		// invisible lock
		void* data;
		vmaMapMemory(mvkobjs.alloc, rvkbucket::globalCameraUBO.alloc, &data);
		std::byte* dst = static_cast<std::byte*>(data);
		std::memcpy(dst, persviewproj.data(), 2 * sizeof(glm::mat4));
		glm::vec4 campos4(mvkobjs.camwpos, 0.0f);
		std::memcpy(dst + (2 * sizeof(glm::mat4)), &campos4, sizeof(glm::vec4));
		vmaUnmapMemory(mvkobjs.alloc, rvkbucket::globalCameraUBO.alloc);
	}

	for (const auto &i : mplayer)
		i->uploadubossbo(mvkobjs, persviewproj, mvkobjs.camwpos);

	mvkobjs.uploadubossbotime = muploadubossbotimer.stop();



	currentgraph.add_pass([&]{
		particle::record_compute(c);
	});



	// currentgraph.add_pass([&mvkobjs, c] {
    //     uint32_t num_instances = mvkobjs.frameInstances.size();
    //     if (num_instances == 0) return;
    //     vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_COMPUTE, rvkbucket::globalcullpline);
    //     vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_COMPUTE,
    //                             rvkbucket::globalPipelineLayout, 0, 1,
    //                             &rvkbucket::globalBindlessSet, 0, nullptr);

    //     vkpushconstants push{};
    //     push.frameIndex = rvkbucket::currentFrame;
    //     vkCmdPushConstants(c, rvkbucket::globalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vkpushconstants), &push);

    //     uint32_t groupCount = (static_cast<uint32_t>(num_instances) + 63) / 64;
    //     vkCmdDispatch(c, groupCount, 1, 1);
    // });


	// let that sync in
	currentgraph.add_pass([&mvkobjs, c, imgidx] {


		VkBufferMemoryBarrier2 particleBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		particleBarrier.buffer = particle::ssbobuffsnallocs.at(0).first;
		particleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		particleBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		particleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		particleBarrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
		particleBarrier.size = VK_WHOLE_SIZE;
		particleBarrier.offset = 0;

		// VkBufferMemoryBarrier2 indirectBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		// indirectBarrier.buffer = mvkobjs.globalIndirectBuffers[rvkbucket::currentFrame].buffer;
		// indirectBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		// indirectBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		// indirectBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
		// indirectBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
		// indirectBarrier.size = VK_WHOLE_SIZE;
		// indirectBarrier.offset = 0;

		VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
		imageBarrier.image = mvkobjs.schainimgs.at(imgidx);
		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		imageBarrier.srcAccessMask = 0;
		imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		VkImageMemoryBarrier2 depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
		depthBarrier.image = mvkobjs.rddepthimage;
		depthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		depthBarrier.srcAccessMask = 0;
		depthBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		depthBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthBarrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

		VkBufferMemoryBarrier2 bufferBarriers[] = {particleBarrier}; // indirectBarrier
		VkImageMemoryBarrier2 imageBarriers[] = {imageBarrier, depthBarrier};

		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.bufferMemoryBarrierCount = 1; // 2
		dep.pBufferMemoryBarriers = bufferBarriers;
		dep.imageMemoryBarrierCount = 2;
		dep.pImageMemoryBarriers = imageBarriers;

		vkCmdPipelineBarrier2(c, &dep);
	});

	currentgraph.add_pass([&mvkobjs, c, imgidx]{


		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(mvkobjs.schain.extent.height);
		viewport.width = static_cast<float>(mvkobjs.schain.extent.width);
		viewport.height = -static_cast<float>(mvkobjs.schain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = mvkobjs.schain.extent;

		
		VkRenderingAttachmentInfo color_attach{};
		color_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color_attach.imageView = mvkobjs.schainimgviews.at(imgidx);
		color_attach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attach.clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		VkRenderingAttachmentInfo depth_attach{};
		depth_attach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attach.imageView = mvkobjs.rddepthimageview;
		depth_attach.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		depth_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attach.clearValue = {.depthStencil = {1.0f, 0}};

		VkRenderingInfo render_info{};
		render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		render_info.renderArea = scissor;
		render_info.layerCount = 1;
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachments = &color_attach;
		render_info.pDepthAttachment = &depth_attach;


		

		vkCmdBeginRendering(c, &render_info);



		vkCmdSetViewport(c, 0, 1,
						&viewport);
		vkCmdSetScissor(c, 0, 1,
						&scissor);


		vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, particle::gpline);

		vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_GRAPHICS, rvkbucket::globalPipelineLayout, 0, 1, &rvkbucket::globalBindlessSet, 0, nullptr);
        

        vkpushconstants push{};
        push.posIdx = particle::bindless_idx;
        vkCmdPushConstants(c, rvkbucket::globalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vkpushconstants), &push);

        vkCmdDraw(c, 8192, 1, 0, 0);



		
		// if (!mvkobjs.frameInstances.empty()) {
            uint32_t indirect_offset = 0;
            for (const auto &player : mplayer) {
                if (player->instcount() > 0) {
                    player->draw(mvkobjs, indirect_offset);
                    indirect_offset += player->instcount();
                }
            }
        // }

        ui::createdbgframe(mvkobjs, selectiondata);
        ui::render(mvkobjs, c);

        vkCmdEndRendering(c);

	});

	
	currentgraph.add_pass([&mvkobjs,c,imgidx]{

		VkImageMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.image = mvkobjs.schainimgs.at(imgidx);
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
		barrier.dstAccessMask = 0;
		barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkDependencyInfo dependency_info{};
		dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency_info.imageMemoryBarrierCount = 1;
		dependency_info.pImageMemoryBarriers = &barrier;

		vkCmdPipelineBarrier2(c, &dependency_info);

	});

	
	currentgraph.execute();

	

	if (vkEndCommandBuffer(c) != VK_SUCCESS)
		return false;



	// for (const auto &player : mplayer) {
	// 	uint32_t modelID = player->get_modelID();
	// 	bool isSkinned = player->is_skinned();
		
	// 	uint32_t stride = isSkinned ? player->getinst(0)->getjointmatrixsize() : 1;

	// 	for (size_t i = 0; i < player->instcount(); ++i) {
	// 		auto inst = player->getinst(i);
	// 		if (!inst->getinstancesettings().msdrawmodel) continue;

	// 		GPUInstanceData idata{};
	// 		idata.worldTransform = inst->calcstaticmat();
	// 		idata.modelID = modelID;
	// 		idata.jointOffset = i * stride;
	// 		idata.isSkinned = isSkinned ? 1 : 0;
	// 		idata.indexCount = player->getindexcount(); 
	// 		idata.firstIndex = player->getfirstindex();
	// 		idata.vertexOffset = player->getvertexoffset();
	// 		idata.padding = 0;

	// 		mvkobjs.frameInstances.push_back(idata);
	// 	}
	// }

	// wtf
	muigentimer.start();
	mvkobjs.rduigeneratetime = muigentimer.stop();





	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::array<VkPipelineStageFlags, 1> waitstage = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		// ,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
	};
	submitinfo.pWaitDstStageMask = waitstage.data();
	std::array<VkSemaphore, 1> waitsemas{
		mvkobjs.semaphorez.at(0).at(rvkbucket::currentFrame)
		                //   ,mvkobjs.semaphorez.at(2).at(rvkbucket::currentFrame)
						};
	submitinfo.waitSemaphoreCount = waitsemas.size();
	submitinfo.pWaitSemaphores = waitsemas.data();

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.semaphorez.at(1).at(imgidx);

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &c;

		vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.fencez.at(rvkbucket::currentFrame));

	{
		std::lock_guard<std::shared_mutex> lock(*mvkobjs.mtx2);
		auto res  = vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo,
		                  mvkobjs.fencez.at(rvkbucket::currentFrame));
						//   vkQueueWaitIdle(mvkobjs.graphicsQ);
						  if(res != VK_SUCCESS){
							std::cout << "QUEUE ERROR : " << res << std::endl; 
							return false;
						  }
						  
	}

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.semaphorez.at(1).at(imgidx);

	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &mvkobjs.schain.swapchain;

	presentinfo.pImageIndices = &imgidx;

	mvkobjs.mtx2->lock();
	res = vkQueuePresentKHR(mvkobjs.presentQ, &presentinfo);

	mvkobjs.mtx2->unlock();

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain(mvkobjs);
	} else {
		if (res != VK_SUCCESS) {
			return false;
		}
	}
	mlasttick = tick;
	rvkbucket::currentFrame =
	    (rvkbucket::currentFrame + 1) % rvkbucket::MAX_FRAMES_IN_FLIGHT;

	return true;
}
