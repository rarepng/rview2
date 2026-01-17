module;
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

#include "core/rvk.hpp"

module rview.rvk.tex;

//todo: use the proper pools, descriptors, layouts and buffers
//after fixing the intellisense problems caused by modules..
namespace rview::rvk::tex {
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

	VkCommandBufferAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = rdata.cpools_graphics.at(0),
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer cmd;
	if (vkAllocateCommandBuffers(rdata.vkdevice.device, &allocInfo, &cmd) != VK_SUCCESS) {
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
			[&](fastgltf::sources::BufferView& view) {
				auto& bv = model.bufferViews[view.bufferViewIndex];
				auto& buf = model.buffers[bv.bufferIndex];
				std::visit(fastgltf::visitor{
					[](auto&) {},
					[&](fastgltf::sources::Array& vec) {
						auto* data_ptr = reinterpret_cast<const stbi_uc*>(vec.bytes.data() + bv.byteOffset);
						pixels = stbi_load_from_memory(data_ptr, static_cast<int>(bv.byteLength), &w, &h, &c, 4);
					}
				}, buf.data);
			},
			[&](fastgltf::sources::Array& vec) {
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
		rdata.mtx2->lock();
		vkQueueSubmit(rdata.graphicsQ, 1, &submit, fence);
		rdata.mtx2->unlock();
	}

	vkWaitForFences(rdata.vkdevice.device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(rdata.vkdevice.device, fence, nullptr);

	vkFreeCommandBuffers(rdata.vkdevice.device, rdata.cpools_graphics.at(0), 1, &cmd);

	for (auto& t : trash_bin) vmaDestroyBuffer(rdata.alloc, t.b, t.a);
	return BatchResult{ (uint32_t)model.images.size(), all_good };
}

bool update_descriptor_set(
    rvkbucket& rdata,
    const std::vector<texdata>& textures,
    VkDescriptorSetLayout& layout,
    VkDescriptorPool& pool,
    VkDescriptorSet& target_set
) {
	VkDevice device = rdata.vkdevice.device;

	//what is this called? lazy inits? todo:find out and reconsider
	if (layout == VK_NULL_HANDLE) {
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1024;//idk
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorBindingFlags bindFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
		VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
		flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		flagsInfo.bindingCount = 1;
		flagsInfo.pBindingFlags = &bindFlag;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = &flagsInfo;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
			std::cout << "[Error] Failed to create Texture Layout!" << std::endl;
			return false;
		}
	}
	if (pool == VK_NULL_HANDLE) {
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1024;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
			std::cout << "[Error] Failed to create Texture Pool!" << std::endl;
			return false;
		}
	}
	if (target_set == VK_NULL_HANDLE) {
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;

		if (vkAllocateDescriptorSets(device, &allocInfo, &target_set) != VK_SUCCESS) {
			std::cout << "[Error] Failed to allocate Texture Set!" << std::endl;
			return false;
		}
	}



	std::vector<VkWriteDescriptorSet> writes;
	VkDescriptorImageInfo dummy0{ .sampler = rdata.samplerz[0], .imageView = rdata.defaults.purple.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo dummy1{ .sampler = rdata.samplerz[0], .imageView = rdata.defaults.white.view,  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo dummy2{ .sampler = rdata.samplerz[0], .imageView = rdata.defaults.normal.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo dummy3{ .sampler = rdata.samplerz[0], .imageView = rdata.defaults.black.view,  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	auto add_write = [&](uint32_t index, VkDescriptorImageInfo* info) {
		writes.push_back(VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = target_set,
			.dstBinding = 0,
			.dstArrayElement = index,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = info
		});
	};

	add_write(0, &dummy0);
	add_write(1, &dummy1);
	add_write(2, &dummy2);
	add_write(3, &dummy3);

	std::vector<VkDescriptorImageInfo> modelInfos;
	modelInfos.reserve(textures.size());

	for (const auto& tex : textures) {
		VkDescriptorImageInfo info{};
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		if (tex.imgview != VK_NULL_HANDLE && tex.imgsampler != VK_NULL_HANDLE) {
			info.imageView = tex.imgview;
			info.sampler = tex.imgsampler;
		} else {
			info.imageView = rdata.defaults.purple.view;
			info.sampler = rdata.samplerz[0];
		}
		modelInfos.push_back(info);
	}

	if (!modelInfos.empty()) {
		writes.push_back(VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = target_set,
			.dstBinding = 0,
			.dstArrayElement = 4,
			.descriptorCount = static_cast<uint32_t>(modelInfos.size()),
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = modelInfos.data()
		});
	}

	if (!writes.empty()) {
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	return true;
}

bool load_env_map(
    rvkbucket& rdata,
    texdata& out_env_map,
    VkDescriptorSet& target_set,
    VkDescriptorSetLayout& target_lay
) {

	int w, h, channels;
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
	rvkbucket::hdrmiplod = mipLevels;
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

//im not sure if this properly sets up the hdr at binding 3 set 0, have to verify.
	VkDescriptorSetLayoutBinding bindInfo{};
	bindInfo.binding = 0;
	bindInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindInfo.descriptorCount = 1;
	bindInfo.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &bindInfo;

	vkCreateDescriptorSetLayout(rdata.vkdevice.device, &layoutInfo, nullptr, &target_lay);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = rdata.dpools[0];
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &target_lay;

	vkAllocateDescriptorSets(rdata.vkdevice.device, &allocInfo, &target_set);



	VkCommandBuffer cmd = rdata.cbuffers_graphics.at(0);
	VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	vkBeginCommandBuffer(cmd, &begin_info);

	VkImageMemoryBarrier barrier0 = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = out_env_map.img,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 } // Transition ALL mips first
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
		vkCmdBlitImage(cmd, out_env_map.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out_env_map.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

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
		rdata.mtx2->lock();
		vkQueueSubmit(rdata.graphicsQ, 1, &submit, fence);
		rdata.mtx2->unlock();
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
	descriptorWrite.dstSet = target_set;
	descriptorWrite.dstBinding = 0;
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
void cleanuptpl(rvkbucket& rdata,    VkDescriptorSetLayout& layout,VkDescriptorPool& pool) {
	vkDestroyDescriptorPool(rdata.vkdevice.device, pool, nullptr);
}
}