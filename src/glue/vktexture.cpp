#include <fastgltf/core.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <ranges>
#include <format>
#include <filesystem>
#include <string>
#include <string_view>

#include "core/rvk.hpp"

#include "vktex.hpp"


//todo: use the proper pools, descriptors, layouts and buffers
//after fixing the intellisense problems caused by modules..
namespace rview::rvk::tex {
texdata upload_texture_async(rvkbucket& rdata, VkCommandBuffer cmd, const uint8_t* pixels, int w, int h) {
	texdata out_tex{};
	out_tex.img = VK_NULL_HANDLE;
	out_tex.imgview = VK_NULL_HANDLE;
	out_tex.imgsampler = VK_NULL_HANDLE;

	VkDeviceSize imgByteSize = w * h * 4;

	VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imgInfo.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo vmaAllocInfo{ .usage = VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(rdata.alloc, &imgInfo, &vmaAllocInfo, &out_tex.img, &out_tex.alloc, nullptr);

	VkDeviceSize beltOffset = rdata.sbelt.reserve(imgByteSize);
	std::memcpy(rdata.sbelt.mappedData + beltOffset, pixels, imgByteSize);
	vmaFlushAllocation(rdata.alloc, rdata.sbelt.allocation, beltOffset, imgByteSize);

	VkImageMemoryBarrier b1{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	b1.srcAccessMask = 0;
	b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	b1.image = out_tex.img;
	b1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b1);

	VkBufferImageCopy region{};
	region.bufferOffset = beltOffset;
	// region.bufferRowLength = 0;
	// region.bufferImageHeight = 0;
	region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
	region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
	vkCmdCopyBufferToImage(cmd, rdata.sbelt.buffer, out_tex.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	VkImageMemoryBarrier b2 = b1;
	b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b2);

	VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	viewInfo.image = out_tex.img;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCreateImageView(rdata.vkdevice.device, &viewInfo, nullptr, &out_tex.imgview);

	return out_tex;
}
[[nodiscard]] static VkImageCreateInfo make_image_info(uint32_t w, uint32_t h) noexcept {
	return VkImageCreateInfo {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.extent = { .width = w, .height = h, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_LINEAR,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
}
BatchResult load_batch(
    rvkbucket& rdata,
    std::vector<texdata>& out_textures,
    fastgltf::Asset& model,
    TextureErrorCallback on_error
) {
	if (model.images.empty()) return {0, true};

	out_textures.clear();

	out_textures.resize(model.images.size());

	uint32_t graphicsQIndex = rdata.vkdevice.get_queue_index(vkb::QueueType::graphics).value();

	VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = graphicsQIndex
	};

	VkCommandPool threadLocalPool;

	if (vkCreateCommandPool(rdata.vkdevice.device, &poolInfo, nullptr, &threadLocalPool) != VK_SUCCESS) {
		return {0, false};
	}

	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = threadLocalPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer cmd;

	if (vkAllocateCommandBuffers(rdata.vkdevice.device, &allocInfo, &cmd) != VK_SUCCESS) {
		vkDestroyCommandPool(rdata.vkdevice.device, threadLocalPool, nullptr);
		return {0, false};
	}

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(cmd, &begin_info);

	struct Staging {
		VkBuffer b;
		VmaAllocation a;
	};
	std::vector<Staging> trash_bin;
	trash_bin.reserve(model.images.size());

	bool all_good = true;

	for (auto [i, img_asset] : std::views::enumerate(model.images)) {

		out_textures[i].img = VK_NULL_HANDLE;
		out_textures[i].imgview = VK_NULL_HANDLE;
		out_textures[i].imgsampler = VK_NULL_HANDLE;

		int w, h, c;
		stbi_uc* pixels = nullptr;

		std::visit(fastgltf::visitor{
			[](auto&) {},
			[&](fastgltf::sources::BufferView & view) {
				auto& bv = model.bufferViews[view.bufferViewIndex];
				auto& buf = model.buffers[bv.bufferIndex];
				std::visit(fastgltf::visitor{
					[](auto&) {},
					[&](fastgltf::sources::Array & vec) {
						auto* data_ptr = reinterpret_cast<const stbi_uc*>(vec.bytes.data() + bv.byteOffset);
						pixels = stbi_load_from_memory(data_ptr, static_cast<int>(bv.byteLength), &w, &h, &c, 4);
					}
				}, buf.data);
			},
			[&](fastgltf::sources::Array & vec) {
				auto* data_ptr = reinterpret_cast<const stbi_uc*>(vec.bytes.data());
				pixels = stbi_load_from_memory(data_ptr, static_cast<int>(vec.bytes.size()), &w, &h, &c, 4);
			}
		}, img_asset.data);

		//todo:reconsider this, maybe the whole module
		if (!pixels) {
			if (on_error) on_error("Failed decode index " + std::to_string(i));

			all_good = false;
			continue;
		}

		VkDeviceSize size = w * h * 4;
		auto info = make_image_info(w, h);
		VmaAllocationCreateInfo alloc_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
		vmaCreateImage(rdata.alloc, &info, &alloc_info, &out_textures[i].img, &out_textures[i].alloc, nullptr);

		// Staging here
		VkBufferCreateInfo stage_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		};
		VmaAllocationCreateInfo stage_alloc_info = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, .usage = VMA_MEMORY_USAGE_AUTO
		};
		VkBuffer stage_buf;
		VmaAllocation stage_alloc;
		vmaCreateBuffer(rdata.alloc, &stage_info, &stage_alloc_info, &stage_buf, &stage_alloc, nullptr);

		void* data;
		vmaMapMemory(rdata.alloc, stage_alloc, &data);
		std::memcpy(data, pixels, size);
		vmaUnmapMemory(rdata.alloc, stage_alloc);
		stbi_image_free(pixels);

		trash_bin.push_back({stage_buf, stage_alloc});

		VkImageMemoryBarrier b1 = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = out_textures[i].img,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b1);

		VkBufferImageCopy copy_region = {
			.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			.imageExtent = { (uint32_t)w, (uint32_t)h, 1 }
		};
		vkCmdCopyBufferToImage(cmd, stage_buf, out_textures[i].img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		VkImageMemoryBarrier b2 = b1;
		b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b2);

		VkImageViewCreateInfo v_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = out_textures[i].img,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_SRGB,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};
		vkCreateImageView(rdata.vkdevice.device, &v_info, nullptr, &out_textures[i].imgview);

		VkSamplerCreateInfo s_info = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR, .maxAnisotropy = 1.0f
		};
		vkCreateSampler(rdata.vkdevice.device, &s_info, nullptr, &out_textures[i].imgsampler);


	}

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
	VkFence fence;
	VkFenceCreateInfo f_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	vkCreateFence(rdata.vkdevice.device, &f_info, nullptr, &fence);

	{
		rview::core::mtx2->lock();
		vkQueueSubmit(rdata.graphicsQ, 1, &submit, fence);
		rview::core::mtx2->unlock();
	}

	vkWaitForFences(rdata.vkdevice.device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(rdata.vkdevice.device, fence, nullptr);

	// vkFreeCommandBuffers(rdata.vkdevice.device, rdata.cpools_graphics.at(1), 1, &cmd);
	vkDestroyCommandPool(rdata.vkdevice.device, threadLocalPool, nullptr);

	for (auto& t : trash_bin) vmaDestroyBuffer(rdata.alloc, t.b, t.a);

	return BatchResult{ (uint32_t)model.images.size(), all_good };
}

bool update_descriptor_set(rvkbucket& rdata, std::vector<texdata>& textures) {

	if (textures.empty()) return true;

	std::vector<VkDescriptorImageInfo> infos(textures.size());
	std::vector<VkWriteDescriptorSet> writes(textures.size());

	for (size_t i = 0; i < textures.size(); ++i) {
		textures[i].bindless_idx = rview::core::globalTextureCounter.fetch_add(1, std::memory_order_relaxed);

		infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		infos[i].imageView = textures[i].imgview ? textures[i].imgview : rview::core::defaults.purple.view;
		infos[i].sampler = textures[i].imgsampler ? textures[i].imgsampler : rdata.samplerz[0];

		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = rview::core::globalBindlessSet;
		writes[i].dstBinding = 0;
		writes[i].dstArrayElement = textures[i].bindless_idx;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &infos[i];
	}

	{
		std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
		vkUpdateDescriptorSets(rdata.vkdevice.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	return true;
}

bool load_env_map(
    rvkbucket& rdata,
    texdata& out_env_map
) {

	int w, h, channels;
	//hardcoded
	std::filesystem::path targetPath = "resources/bg.hdr";

	std::cout << "[Debug] Current Working Directory: " << std::filesystem::current_path() << std::endl;

	if (!std::filesystem::exists(targetPath)) {
		std::cout << "[Error] File DOES NOT EXIST at: "
		          << std::filesystem::absolute(targetPath) << std::endl;
		std::cout << "        (Did you forget to copy the 'resources' folder to the build/bin folder?)" << std::endl;
		return false;
	}

	float* pixels = stbi_loadf(targetPath.string().c_str(), &w, &h, &channels, 4);

	if (!pixels) {
		std::cout << "[Error] stbi_loadf failed! Reason: " << stbi_failure_reason() << std::endl;
		return false;
	}

	std::cout << "[Success] Loaded EXR: " << w << "x" << h << std::endl;

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
	rview::core::hdrmiplod = mipLevels;
	VkDeviceSize imageSize = w * h * 4 * sizeof(float);

	VkImageCreateInfo imageInfo {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.extent = { (uint32_t)w, (uint32_t)h, 1 },
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo alloc_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
	vmaCreateImage(rdata.alloc, &imageInfo, &alloc_info, &out_env_map.img, &out_env_map.alloc, nullptr);

	VkBuffer stage_buf;
	VmaAllocation stage_alloc;
	VkBufferCreateInfo stage_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = imageSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};
	VmaAllocationCreateInfo stage_alloc_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	vmaCreateBuffer(rdata.alloc, &stage_info, &stage_alloc_info, &stage_buf, &stage_alloc, nullptr);

	void* data;
	vmaMapMemory(rdata.alloc, stage_alloc, &data);
	std::memcpy(data, pixels, static_cast<size_t>(imageSize));
	vmaUnmapMemory(rdata.alloc, stage_alloc);
	stbi_image_free(pixels);


	VkCommandBuffer cmd = rdata.cbuffers_graphics.at(1).at(rview::core::currentFrame);
	VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	vkBeginCommandBuffer(cmd, &begin_info);

	VkImageMemoryBarrier barrier0 = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = out_env_map.img,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
	};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier0);

	VkBufferImageCopy copyRegion = {
		.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
		.imageExtent = { (uint32_t)w, (uint32_t)h, 1 }
	};
	vkCmdCopyBufferToImage(cmd, stage_buf, out_env_map.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	int32_t mipW = w;
	int32_t mipH = h;

	for (uint32_t i = 1; i < mipLevels; i++) {
		VkImageMemoryBarrier barrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.image = out_env_map.img,
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 }
		};
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkImageBlit blit = {
			.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 },
			.srcOffsets = { {0, 0, 0}, {mipW, mipH, 1} },
			.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 },
			.dstOffsets = { {0, 0, 0}, { mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1 } }
		};
		vkCmdBlitImage(cmd, out_env_map.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out_env_map.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
		               VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		if (mipW > 1) mipW /= 2;

		if (mipH > 1) mipH /= 2;
	}

	VkImageMemoryBarrier barrierLast = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.image = out_env_map.img,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 1 }
	};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierLast);

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
	VkFence fence;
	VkFenceCreateInfo f_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	vkCreateFence(rdata.vkdevice.device, &f_info, nullptr, &fence);

	{
		rview::core::mtx2->lock();
		vkQueueSubmit(rdata.graphicsQ, 1, &submit, fence);
		rview::core::mtx2->unlock();
	}
	vkWaitForFences(rdata.vkdevice.device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(rdata.vkdevice.device, fence, nullptr);
	vmaDestroyBuffer(rdata.alloc, stage_buf, stage_alloc);

	VkImageViewCreateInfo v_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = out_env_map.img,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 }
	};
	vkCreateImageView(rdata.vkdevice.device, &v_info, nullptr, &out_env_map.imgview);

	VkSamplerCreateInfo s_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.maxAnisotropy = 16.0f,
		.minLod = 0.0f,
		.maxLod = static_cast<float>(mipLevels),
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
	};
	vkCreateSampler(rdata.vkdevice.device, &s_info, nullptr, &out_env_map.imgsampler);

	VkDescriptorImageInfo descimageInfo{};
	descimageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descimageInfo.imageView = out_env_map.imgview;
	descimageInfo.sampler = out_env_map.imgsampler;

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = rview::core::globalBindlessSet;
	descriptorWrite.dstBinding = 3;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &descimageInfo;

	vkUpdateDescriptorSets(rdata.vkdevice.device, 1, &descriptorWrite, 0, nullptr);
	return true;
}

void cleanup(rvkbucket& rdata, texdata& tex) {
	if (tex.imgsampler) vkDestroySampler(rdata.vkdevice.device, tex.imgsampler, nullptr);

	if (tex.imgview)    vkDestroyImageView(rdata.vkdevice.device, tex.imgview, nullptr);

	if (tex.img)        vmaDestroyImage(rdata.alloc, tex.img, tex.alloc);
}

//useless
void cleanuptpl(rvkbucket& rdata,    VkDescriptorSetLayout& layout, VkDescriptorPool& pool) {
	vkDestroyDescriptorPool(rdata.vkdevice.device, pool, nullptr);
}
}