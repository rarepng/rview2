#include <backends/imgui_impl_sdl3.h>
#include <core/rvk.hpp>
#include <cstdint>
#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION
#include <vkrenderer.hpp>
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
#include <vk/commandbuffer.hpp>
#include <vk/commandpool.hpp>
#include <vksyncobjects.hpp>
#include <SDL3/SDL_vulkan.h>
#include <vktex.hpp>
#include <playout.hpp>
#include <ubo.hpp>
#include <obs_bridge.hpp>

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

	rview::core::mtx2->lock();
	vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitInfo, tempFence);
	rview::core::mtx2->unlock();

	vkWaitForFences(mvkobjs.vkdevice.device, 1, &tempFence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(mvkobjs.vkdevice.device, tempFence, nullptr);

	vkFreeCommandBuffers(mvkobjs.vkdevice.device, mvkobjs.cpools_graphics.at(0),
	                     1, &cbuffer);
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

	g_exitQ.enqueue(TeardownPhase::device, [device = objs.vkdevice.device,
	sampler = objs.samplerz[0]]() {
		vkDestroySampler(device, sampler, nullptr);
	});


	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if (vkCreateSampler(objs.vkdevice.device, &info, nullptr, &objs.samplerz[1]) != VK_SUCCESS) {
		return false;
	}

	g_exitQ.enqueue(TeardownPhase::device, [device = objs.vkdevice.device,
	sampler = objs.samplerz[1]]() {
		vkDestroySampler(device, sampler, nullptr);
	});

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
	                         VkMemoryPropertyFlags properties, VkBuffer & buffer,
	VkDeviceMemory & bufferMemory) {
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

	auto create_1x1_texture = [&](uint32_t pixelData) -> rview::core::DummyTexture {
		rview::core::DummyTexture tex{};

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
		void* data;
		vkMapMemory(objs.vkdevice.device, stagingMem, 0, imageSize, 0, &data);
		memcpy(data, &pixelData, (size_t)imageSize);
		vkUnmapMemory(objs.vkdevice.device, stagingMem);

		vkrenderer::immediate_submit(objs, [&](VkCommandBuffer cmd) {
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

		g_exitQ.enqueue(TeardownPhase::device, [device = objs.vkdevice.device,
		                                        img = tex.image,
		                                        view = tex.view,
		mem = tex.memory]() {
			vkDestroyImageView(device, view, nullptr);
			vkDestroyImage(device, img, nullptr);
			vkFreeMemory(device, mem, nullptr);
		});

		return tex;
	};

	rview::core::defaults.purple = create_1x1_texture(0xDD00DDDD); // error texture
	rview::core::defaults.white = create_1x1_texture(0xFFFFFFFF);
	rview::core::defaults.normal = create_1x1_texture(0xFFFF8080);
	rview::core::defaults.black = create_1x1_texture(0xFF000000);

	std::array<VkDescriptorImageInfo, 4> dummyInfos{};
	std::array<VkWriteDescriptorSet, 4> dummyWrites{};

	dummyInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[0].imageView = rview::core::defaults.purple.view;
	dummyInfos[0].sampler = objs.samplerz[0];

	dummyInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[1].imageView = rview::core::defaults.white.view;
	dummyInfos[1].sampler = objs.samplerz[0];

	dummyInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[2].imageView = rview::core::defaults.normal.view;
	dummyInfos[2].sampler = objs.samplerz[0];

	dummyInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dummyInfos[3].imageView = rview::core::defaults.black.view;
	dummyInfos[3].sampler = objs.samplerz[0];

	for (uint32_t i = 0; i < 4; ++i) {
		dummyWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		dummyWrites[i].dstSet = rview::core::globalBindlessSet;
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

	g_commit_queue.reserve(64);

	if (!initvma(mvkobjs)) return false;

	if (!getqueue(mvkobjs)) return false;

	if (!createswapchain(mvkobjs)) return false;

	if (!createdepthbuffer(mvkobjs)) return false;

	if (!createcommandpool(mvkobjs)) return false;

	if (!createcommandbuffer(mvkobjs)) return false;

	if (!createsyncobjects(mvkobjs)) return false;

	if (!initcpuQs(mvkobjs)) return false;

	model_manager::reserve_mega_buffers(rview::anim::megabuffers);

	if (!playout::init_bindless(mvkobjs)) return false;

	if (!initglobalmats(mvkobjs)) return false;

	model_manager::init_gpu_joint_buffers(mvkobjs);

	if (!playout::init_pipelines(mvkobjs)) return false;

	if (!init_global_samplers(mvkobjs)) return false;

	if (!init_dummy_textures(mvkobjs, mvkobjs.cpools_graphics.at(0))) return false;

	if (!rview::rvk::tex::load_env_map(mvkobjs, rview::core::exrtex.at(0))) return false;

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);

	if (!ubo::init_global(mvkobjs)) return false;

	if (!initglobalinstances(mvkobjs))return false;

	if (!initglobalmorphs(mvkobjs)) return false;

	if (!initglobalindirect(mvkobjs))return false;

	if (!initglobalidrectUNINDEXED(mvkobjs))return false;

	if (!create_obs_target(mvkobjs)) return false;

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
	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; ++i) {
		if (!mvkobjs.sbelts[i].init(mvkobjs.alloc, 256 * 1024 * 1024)) return false;
	}

	return true;
}
bool vkrenderer::initscene(rvkbucket& mvkobjs) {
	if constexpr (rdemo::is_active) {
		const auto& scene = rdemo::SCENES[1];

		for (const auto& spawn : scene.spawns) {
			const char* fname = g_job_strings.push_string(spawn.filepath);
			uint32_t count = spawn.count;
			glm::vec3 origin = spawn.origin;
			auto modifier = spawn.modifier;

			active_io_jobs.fetch_add(1, std::memory_order_release);
			g_jobs.enqueue([fname, count, origin, modifier]() {
				model_manager::StagingModelData staging = model_manager::parse_model_to_staging(fname);
				staging.requested_instances = count;
				staging.spawn_position = origin;
				staging.skipui = true;
				staging.demo_modifier = modifier;

				if (!staging.meshes.empty()) {
					while (!vkrenderer::pending_staging_models.push(std::move(staging))) {
						std::this_thread::yield();
					}
				}

				active_io_jobs.fetch_sub(1, std::memory_order_release);
			});
		}

		return true; // will skip boot.json for nows
	}

	simdjson::ondemand::parser p;
	simdjson::padded_string j;

	if (auto error = simdjson::padded_string::load("boot.json").get(j); error) {
		std::cerr << "X_X " << error << std::endl;
		return false;
	}

	simdjson::ondemand::document d;

	if (auto error = p.iterate(j).get(d); error) return false;

	simdjson::ondemand::array models;

	if (d["models"].get(models)) return false;

	for (simdjson::ondemand::object x : models) {
		std::string_view f = x["file"].get_string();
		uint64_t c = x["count"].get_uint64();
		// POSITION NOT OPTIONAL AND MUST BE 3
		// no x["position"].get(pos_arr) == simdjson::SUCCESS
		simdjson::ondemand::array pos_arr = x["position"].get_array();
		auto it = pos_arr.begin();
		float pX = static_cast<float>((*it).get_double());
		++it;
		float pY = static_cast<float>((*it).get_double());
		++it;
		float pZ = static_cast<float>((*it).get_double());
		glm::vec3 start_pos(pX, pY, pZ);
		uint32_t instance_count = static_cast<uint32_t>(c);
		const char* fname = g_job_strings.push_string(f);
		active_io_jobs.fetch_add(1, std::memory_order_release);

		g_jobs.enqueue([fname, instance_count, start_pos]() {
			model_manager::StagingModelData staging = model_manager::parse_model_to_staging(fname);
			staging.requested_instances = instance_count;
			staging.dropID = 0xFFFFFFFF; // irrelevant
			staging.skipui = true;
			staging.spawn_position = start_pos;

			if (!staging.meshes.empty()) {
				while (!vkrenderer::pending_staging_models.push(std::move(staging))) {
					if constexpr (rdebug::is_active) {
						std::cerr << "Engine queue full! Dropped boot model: " << fname << "\n";
					}

					std::this_thread::yield();
				}
			} else {
				std::cerr << "Failed to parse boot model: " << fname << "\n";
			}

			active_io_jobs.fetch_sub(1, std::memory_order_release);
		});
	}

	return true;
}
bool vkrenderer::initglobalmats(rvkbucket &mvkobjs) {
	size_t bufferSize = rview::core::MAX_GLOBAL_MATERIALS * sizeof(MaterialData);
	VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	binfo.size = bufferSize;
	binfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VmaAllocationCreateInfo vmaallocinfo{};
	vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	vmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocResult;

	if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo,
	                    &rview::core::global_materials.buffer,
	                    &rview::core::global_materials.alloc, &allocResult) != VK_SUCCESS) {
		return false;
	}

	rview::core::global_materials.mapped_data = allocResult.pMappedData;
	model_manager::MaterialData* mats = static_cast<model_manager::MaterialData*>(allocResult.pMappedData);

	for (uint32_t i = 0; i < rview::core::MAX_GLOBAL_MATERIALS; ++i) {
		mats[i].albedoIdx = 0xFFFFFFFF;
		mats[i].normalIdx = 0xFFFFFFFF;
		mats[i].ormIdx = 0xFFFFFFFF;
		mats[i].emissiveIdx = 0xFFFFFFFF;
		mats[i].transmissionIdx = 0xFFFFFFFF;
		mats[i].sheenIdx = 0xFFFFFFFF;
		mats[i].clearcoatIdx = 0xFFFFFFFF;
		mats[i].thicknessIdx = 0xFFFFFFFF;

		mats[i].baseColorFactor = glm::vec4(1.0f);
		mats[i].emissiveFactor = glm::vec3(0.0f);
		mats[i].normalScale = 1.0f;
		mats[i].roughnessFactor = 1.0f;
		mats[i].metallicFactor = 0.0f;
	}

	for (uint32_t i = 0; i < rview::core::MAX_GLOBAL_MATERIALS; ++i) {
		rview::core::global_materials.free_slots.push(i);
	}

	VkDescriptorBufferInfo ssboInfo{ rview::core::global_materials.buffer, 0, bufferSize };
	VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	write.dstSet = rview::core::globalBindlessSet;
	write.dstBinding = 4;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.descriptorCount = 1;
	write.pBufferInfo = &ssboInfo;

	vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &write, 0, nullptr);
	return true;
}
bool vkrenderer::initglobalinstances(rvkbucket& mvkobjs) {
	rview::core::frameInstances.reserve(SceneData::MAX_ENTITIES);

	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkBufferCreateInfo ibinfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		ibinfo.size = sizeof(GPUInstanceData) * SceneData::MAX_ENTITIES;
		ibinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		VmaAllocationCreateInfo ivmaallocinfo{};
		ivmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		ivmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (vmaCreateBuffer(mvkobjs.alloc, &ibinfo, &ivmaallocinfo,
		                    &rview::core::globalInstanceBuffers[i].buffer,
		                    &rview::core::globalInstanceBuffers[i].alloc, nullptr) != VK_SUCCESS) {
			return false;
		}

		VkDescriptorBufferInfo issboInfo{rview::core::globalInstanceBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet iwrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		iwrite.dstSet = rview::core::globalBindlessSet;
		iwrite.dstBinding = 7;
		iwrite.dstArrayElement = i;
		iwrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		iwrite.descriptorCount = 1;
		iwrite.pBufferInfo = &issboInfo;

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &iwrite, 0, nullptr);
	}

	return true;
}
bool vkrenderer::initglobalindirect(rvkbucket& mvkobjs) {
	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkBufferCreateInfo ibinfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		ibinfo.size = sizeof(VkDrawIndexedIndirectCommand) * SceneData::MAX_ENTITIES;
		ibinfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

		VmaAllocationCreateInfo ivmaallocinfo{};
		ivmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		ivmaallocinfo.flags = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // VMA_MEMORY_USAGE_GPU_ONLY?

		if (vmaCreateBuffer(mvkobjs.alloc, &ibinfo, &ivmaallocinfo,
		                    &rview::core::globalIndirectBuffers[i].buffer,
		                    &rview::core::globalIndirectBuffers[i].alloc, nullptr) != VK_SUCCESS) {
			return false;
		}

		VkDescriptorBufferInfo issboInfo{rview::core::globalIndirectBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet iwrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		iwrite.dstSet = rview::core::globalBindlessSet;
		iwrite.dstBinding = 8;
		iwrite.dstArrayElement = i;
		iwrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		iwrite.descriptorCount = 1;
		iwrite.pBufferInfo = &issboInfo;

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &iwrite, 0, nullptr);
	}

	return true;
}
bool vkrenderer::initglobalidrectUNINDEXED(rvkbucket& mvkobjs) {
	uint32_t maxDrawCommands = 100000;

	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; i++) {

		if (!vkvbo::init(mvkobjs, rview::core::g_indirectCommandBuffers[i],
		                 maxDrawCommands * sizeof(VkDrawIndirectCommand),
		                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)) {
			return false;
		}

		VkDescriptorBufferInfo cmdInfo{rview::core::g_indirectCommandBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet cmdWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		cmdWrite.dstSet = rview::core::globalBindlessSet;
		cmdWrite.dstBinding = 15;
		cmdWrite.dstArrayElement = i;
		cmdWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		cmdWrite.descriptorCount = 1;
		cmdWrite.pBufferInfo = &cmdInfo;
		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &cmdWrite, 0, nullptr);

		if (!vkvbo::init(mvkobjs, rview::core::g_indirectCountBuffers[i],
		                 sizeof(uint32_t),
		                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
			return false;
		}

		VkDescriptorBufferInfo countInfo{rview::core::g_indirectCountBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet countWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		countWrite.dstSet = rview::core::globalBindlessSet;
		countWrite.dstBinding = 16;
		countWrite.dstArrayElement = i;
		countWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		countWrite.descriptorCount = 1;
		countWrite.pBufferInfo = &countInfo;
		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &countWrite, 0, nullptr);
	}

	return true;
}
void vkrenderer::update_dynamic_instances(rvkbucket& mvkobjs) {
	uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

	if (active_instances == 0) return;

	uint32_t safe_instances = std::min(active_instances, 65535U);

	void* mappedData;
	vmaMapMemory(mvkobjs.alloc, rview::core::globalInstanceBuffers[rview::core::currentFrame].alloc, &mappedData);
	GPUInstanceData* gpuInsts = static_cast<GPUInstanceData*>(mappedData);

	void* weightData;
	vmaMapMemory(mvkobjs.alloc, g_morphWeightBuffers[rview::core::currentFrame].alloc, &weightData);
	float* mappedWeights = static_cast<float*>(weightData);
	uint32_t globalWeightOffset = 0;

	for (uint32_t i = 0; i < safe_instances; ++i) {
		if (!model_manager::g_registry.is_visible[i] || g_scene.modelIDs[i] == 0xFFFFFFFF) {
			gpuInsts[i].modelID = 0xFFFFFFFF;
			continue;
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), g_scene.worldPositions[i]) *
		                      glm::toMat4(g_scene.rotations[i]) *
		                      glm::scale(glm::mat4(1.0f), g_scene.scales[i]);

		gpuInsts[i].worldTransform = transform;
		gpuInsts[i].modelID        = g_scene.modelIDs[i];

		gpuInsts[i].jointOffset    = g_scene.jointOffsets[i];
		gpuInsts[i].isSkinned      = g_scene.isSkinned[i];

		gpuInsts[i].indexCount     		= 0;
		gpuInsts[i].firstIndex     		= 0;
		gpuInsts[i].vertexOffset   		= 0;
		uint32_t wCount = static_cast<uint32_t>(model_manager::g_registry.morph_weights[i].size());
		gpuInsts[i].morphWeightOffset = globalWeightOffset;

		if (wCount > 0) {
			std::memcpy(mappedWeights + globalWeightOffset, model_manager::g_registry.morph_weights[i].data(), wCount * sizeof(float));
			globalWeightOffset += wCount;
		}
	}

	vmaFlushAllocation(mvkobjs.alloc, rview::core::globalInstanceBuffers[rview::core::currentFrame].alloc, 0, safe_instances * sizeof(GPUInstanceData));
	vmaUnmapMemory(mvkobjs.alloc, rview::core::globalInstanceBuffers[rview::core::currentFrame].alloc);

	vmaFlushAllocation(mvkobjs.alloc, g_morphWeightBuffers[rview::core::currentFrame].alloc, 0, globalWeightOffset * sizeof(float));
	vmaUnmapMemory(mvkobjs.alloc, g_morphWeightBuffers[rview::core::currentFrame].alloc);
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
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_graphics, vkb::QueueType::graphics)) [[unlikely]] {
		return false;
	}
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_compute, vkb::QueueType::compute)) [[unlikely]] {
		return false;
	}
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_transfer, vkb::QueueType::transfer)) [[unlikely]] {
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
bool vkrenderer::initglobalmorphs(rvkbucket& mvkobjs) {
	size_t bufferSize = rview::anim::maxmorphs * sizeof(float);

	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		binfo.size = bufferSize;
		binfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		VmaAllocationCreateInfo vmaallocinfo{};
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		vmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo,
		                    &g_morphWeightBuffers[i].buffer,
		                    &g_morphWeightBuffers[i].alloc, nullptr) != VK_SUCCESS) {
			return false;
		}

		VkDescriptorBufferInfo ssboInfo{g_morphWeightBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = rview::core::globalBindlessSet;
		write.dstBinding = 18;
		write.dstArrayElement = i;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &ssboInfo;

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &write, 0, nullptr);
	}

	return true;
}
void vkrenderer::write_obs(rvkbucket& mvkobjs) {
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = mvkobjs.obs_target.img;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(mvkobjs.cbuffers_graphics.at(0).at(rview::core::currentFrame),
	                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                     0, 0, nullptr, 0, nullptr, 1, &barrier);

	VkClearColorValue clearColor = {{1.0f, 0.0f, 1.0f, 1.0f}};
	VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdClearColorImage(mvkobjs.cbuffers_graphics.at(0).at(rview::core::currentFrame), mvkobjs.obs_target.img,
	                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	vkCmdPipelineBarrier(mvkobjs.cbuffers_graphics.at(0).at(rview::core::currentFrame),
	                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
	                     0, 0, nullptr, 0, nullptr, 1, &barrier);
}
void vkrenderer::cleanup(rvkbucket& mvkobjs) {

	rview::io::save_state_to_json();

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);
	ui::cleanup(mvkobjs);
	vksyncobjects::cleanup(mvkobjs);

	for (auto &x : mvkobjs.dpools)
		rpool::destroy(mvkobjs.vkdevice.device, x);

	playout::cleanup_pipelines(mvkobjs);

	model_manager::StagingModelData temp;

	while (pending_staging_models.pop(temp)) {}

	//temp
	mvkobjs.cleanupQ.flush();
	mvkobjs.deletionQ.flush();

	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview,
	                   nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage,
	                mvkobjs.rddepthimagealloc);

	// NO VMA!!!! (idk if it's better or not but i couldnt figure out how to make it work without creating a dedicated separate device and pool)
	if (mvkobjs.obs_target.img) {
		vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.obs_target.view, nullptr);
		vkDestroyImage(mvkobjs.vkdevice.device, mvkobjs.obs_target.img, nullptr);
		vkFreeMemory(mvkobjs.vkdevice.device, mvkobjs.obs_target.memory, nullptr);
	}

	if constexpr (rview::core::platform::is_windows) {
		// cleanup_obs_bridge();
	}

	if (rview::core::exrtex.at(0).img) {
		vkDestroyImageView(mvkobjs.vkdevice.device, rview::core::exrtex.at(0).imgview,
		                   nullptr);
		vkDestroySampler(mvkobjs.vkdevice.device, rview::core::exrtex.at(0).imgsampler,
		                 nullptr);
		vmaDestroyImage(mvkobjs.alloc, rview::core::exrtex.at(0).img,
		                rview::core::exrtex.at(0).alloc);
	}

	vmaDestroyBuffer(mvkobjs.alloc, rview::core::global_materials.buffer, rview::core::global_materials.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, rview::core::globalCameraUBO.buffer, rview::core::globalCameraUBO.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, rview::core::g_rawIndexSSBO.buffer, rview::core::g_rawIndexSSBO.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, rview::core::g_primitiveRegistrySSBO.buffer, rview::core::g_primitiveRegistrySSBO.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, rview::core::g_modelRegistrySSBO.buffer, rview::core::g_modelRegistrySSBO.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, g_morphDeltaSSBO.buffer, g_morphDeltaSSBO.alloc);

	for (auto&& x : rview::core::g_indirectCommandBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : rview::core::g_indirectCountBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : rview::core::globalInstanceBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : rview::core::globalIndirectBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : model_manager::g_gpu_joint_buffers) {
		if (x.buffer) vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);
	}

	for (auto&& x : g_morphWeightBuffers) {
		if (x.buffer) vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);
	}

	for (auto& [id, asset] : model_manager::g_cpuModels) {
		if (asset.geometryVBO.buffer) {
			vmaDestroyBuffer(mvkobjs.alloc, asset.geometryVBO.buffer, asset.geometryVBO.alloc);
		}
	}

	vkDestroyDescriptorPool(mvkobjs.vkdevice.device, rview::core::global_materials.dedicated_pool, VK_NULL_HANDLE);

	for (const auto& i : model_manager::g_cpuModels) {
		for (const auto& j : i.second.textures) {
			vmaDestroyImage(mvkobjs.alloc, j.img, j.alloc);
			vkDestroyImageView(mvkobjs.vkdevice.device, j.imgview, nullptr);
		}
	}

	model_manager::g_cpuModels.clear();
	g_exitQ.flush_and_die();
	std::ranges::for_each(mvkobjs.sbelts, [&mvkobjs](auto & x) {
		x.free(mvkobjs.alloc);
	});

	if constexpr (rdebug::is_active) {
		char* statsString = nullptr;
		vmaBuildStatsString(mvkobjs.alloc, &statsString, true);

		if (statsString) {
			std::cout << "=== VMA REPORT !! ===" << std::endl;
			std::cout << statsString << std::endl;
			std::cout << "=======================" << std::endl;
			vmaFreeStatsString(mvkobjs.alloc, statsString);
		}
	}

	vmaDestroyAllocator(mvkobjs.alloc);
	mvkobjs.schain.destroy_image_views(mvkobjs.schainimgviews);
	vkb::destroy_swapchain(mvkobjs.schain);
	vkb::destroy_device(mvkobjs.vkdevice);
	vkb::destroy_surface(mvkobjs.inst.instance, mvkobjs.surface);
	vkb::destroy_instance(mvkobjs.inst);
}

void vkrenderer::setsize(rvkbucket& mvkobjs, unsigned int w, unsigned int h) {
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
		                         rview::core::fov,
		                         static_cast<float>(mvkobjs.width) / static_cast<float>(mvkobjs.height),
		                         0.002f, 2000.0f);
}
bool vkrenderer::create_obs_target(rvkbucket& mvkobjs) {
	VkPhysicalDeviceIDProperties idProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
	VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	props2.pNext = &idProps;
	vkGetPhysicalDeviceProperties2(mvkobjs.vkdevice.physical_device, &props2);

	if (!idProps.deviceLUIDValid) {
		std::cerr << "Vulkan side: Device lacks valid LUID." << std::endl;
		return false;
	}

	mvkobjs.obs_target.shared_handle = obs_bridge::create_d3d_shared_texture(
	                                       mvkobjs.obs_target.width,
	                                       mvkobjs.obs_target.height,
	                                       idProps.deviceLUID
	                                   );

	if (!mvkobjs.obs_target.shared_handle) return false;

	VkExternalMemoryHandleTypeFlagBitsKHR handleType = obs_bridge::use_d3d12 ?
	    VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHR :
	    VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;

	VkExternalMemoryImageCreateInfo extMemInfo{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
	extMemInfo.handleTypes = handleType;

	VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	imageInfo.pNext = &extMemInfo;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
	imageInfo.extent = { mvkobjs.obs_target.width, mvkobjs.obs_target.height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
	                  VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (vkCreateImage(mvkobjs.vkdevice.device, &imageInfo, nullptr, &mvkobjs.obs_target.img) != VK_SUCCESS) {
		return false;
	}

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(mvkobjs.vkdevice.device, mvkobjs.obs_target.img, &memReqs);

	void* os_import_info = obs_bridge::get_vulkan_import_info(mvkobjs.obs_target.shared_handle);

	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
	dedicatedAllocInfo.pNext = os_import_info;
	dedicatedAllocInfo.image = mvkobjs.obs_target.img;
	dedicatedAllocInfo.buffer = VK_NULL_HANDLE;

	auto find_memory_type = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(mvkobjs.vkdevice.physical_device, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		return 0;
	};

	VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.pNext = &dedicatedAllocInfo;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(mvkobjs.vkdevice.device, &allocInfo, nullptr, &mvkobjs.obs_target.memory) != VK_SUCCESS) {
		return false;
	}

	vkBindImageMemory(mvkobjs.vkdevice.device, mvkobjs.obs_target.img, mvkobjs.obs_target.memory, 0);

	VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	viewInfo.image = mvkobjs.obs_target.img;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(mvkobjs.vkdevice.device, &viewInfo, nullptr, &mvkobjs.obs_target.view) != VK_SUCCESS) {
		return false;
	}

	return true;
}
void vkrenderer::copy_engine_to_obs(VkCommandBuffer cmdBuffer, rvkbucket& mvkobjs, VkImage engine_source_img, VkImage obs_target_img, uint32_t width,
                                    uint32_t height) {

	VkImageMemoryBarrier srcBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	srcBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.image = engine_source_img;
	srcBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	srcBarrier.srcAccessMask = 0;
	srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	VkImageMemoryBarrier dstBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.image = obs_target_img;
	dstBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	dstBarrier.srcAccessMask = 0;
	dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	VkImageMemoryBarrier barriers[] = {srcBarrier, dstBarrier};
	vkCmdPipelineBarrier(cmdBuffer,
	                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                     0, 0, nullptr, 0, nullptr, 2, barriers);

	VkImageBlit blit{};
	blit.srcOffsets[0] = {0, 0, 0};
	blit.srcOffsets[1] = {(int32_t)mvkobjs.schain.extent.width, (int32_t)mvkobjs.schain.extent.height, 1};
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.layerCount = 1;
	blit.dstOffsets[0] = {0, 0, 0};
	blit.dstOffsets[1] = {(int32_t)width, (int32_t)height, 1}; // streeeeeeetch
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.layerCount = 1;

	vkCmdBlitImage(cmdBuffer,
	               engine_source_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	               obs_target_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	               1, &blit, VK_FILTER_LINEAR);

	srcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	srcBarrier.dstAccessMask = 0;
	dstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

	VkImageMemoryBarrier postBarriers[] = {srcBarrier, dstBarrier};
	vkCmdPipelineBarrier(cmdBuffer,
	                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
	                     0, 0, nullptr, 0, nullptr, 2, postBarriers);
}
// Vulkan to DX (MASSIVE FAILURE)
// bool vkrenderer::create_obs_target(rvkbucket& mvkobjs) {
//     VkExternalMemoryImageCreateInfo extMemInfo{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
//     extMemInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT_KHR;
//     VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
//     imageInfo.pNext = &extMemInfo;
//     imageInfo.imageType = VK_IMAGE_TYPE_2D;
//     imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
//     imageInfo.extent = { mvkobjs.obs_target.width, mvkobjs.obs_target.height, 1 };
//     imageInfo.mipLevels = 1;
//     imageInfo.arrayLayers = 1;
//     imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//     imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//     imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//     if (vkCreateImage(mvkobjs.vkdevice.device, &imageInfo, nullptr, &mvkobjs.obs_target.img) != VK_SUCCESS) {
//         return false;
//     }
//     auto find_memory_type = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t {
//         VkPhysicalDeviceMemoryProperties memProperties;
//         vkGetPhysicalDeviceMemoryProperties(mvkobjs.vkdevice.physical_device, &memProperties);
//         for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
//             if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
//                 return i;
//             }
//         }
//         return 0;
//     };
// 	VkMemoryRequirements memReqs;
// 	vkGetImageMemoryRequirements(mvkobjs.vkdevice.device, mvkobjs.obs_target.img, &memReqs);
// 	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
// 	dedicatedAllocInfo.image = mvkobjs.obs_target.img;
// 	dedicatedAllocInfo.buffer = VK_NULL_HANDLE;
// 	void* os_export_info = vkrenderer::get_os_export_memory_info();
// 	VkExportMemoryAllocateInfo exportAllocInfo{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
// 	exportAllocInfo.pNext = os_export_info;
// 	exportAllocInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT_KHR;
// 	dedicatedAllocInfo.pNext = &exportAllocInfo;
// 	VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
// 	allocInfo.pNext = &dedicatedAllocInfo;
// 	allocInfo.allocationSize = memReqs.size;
// 	allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
// 	if (vkAllocateMemory(mvkobjs.vkdevice.device, &allocInfo, nullptr, &mvkobjs.obs_target.memory) != VK_SUCCESS) {
// 		return false;
// 	}
// 	vkBindImageMemory(mvkobjs.vkdevice.device, mvkobjs.obs_target.img, mvkobjs.obs_target.memory, 0);
//     VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
//     viewInfo.image = mvkobjs.obs_target.img;
//     viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//     viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
//     viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     viewInfo.subresourceRange.baseMipLevel = 0;
//     viewInfo.subresourceRange.levelCount = 1;
//     viewInfo.subresourceRange.baseArrayLayer = 0;
//     viewInfo.subresourceRange.layerCount = 1;
//     if (vkCreateImageView(mvkobjs.vkdevice.device, &viewInfo, nullptr, &mvkobjs.obs_target.view) != VK_SUCCESS) {
//         return false;
//     }
// 	mvkobjs.obs_target.shared_handle = vkrenderer::extract_vulkan_win32_handle(mvkobjs.vkdevice.device, mvkobjs.obs_target.memory);
//     if (!mvkobjs.obs_target.shared_handle) {
//         std::cerr << "[Fatal] Vulkan side: Failed to extract NT handle." << std::endl;
//         return false;
//     }
//     VkPhysicalDeviceIDProperties idProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
//     VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
//     props2.pNext = &idProps;
//     vkGetPhysicalDeviceProperties2(mvkobjs.vkdevice.physical_device, &props2);
//     if (!idProps.deviceLUIDValid) {
//         std::cerr << "[Fatal] Vulkan side: Device lacks valid LUID." << std::endl;
//         return false;
//     }
//     if (!init_obs_bridge(mvkobjs.obs_target.shared_handle, mvkobjs.obs_target.width, mvkobjs.obs_target.height, idProps.deviceLUID)) {
//         return false;
//     }
//     return true;
// }

void vkrenderer::sdlevent(rvkbucket& mvkobjs, const SDL_Event& e) {
	switch (e.type) {
		case SDL_EVENT_KEY_DOWN:
			[[likely]]
		case SDL_EVENT_KEY_UP:
			[[likely]] {
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
			[[unlikely]] {
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
			[[unlikely]] {
				std::string_view fname_view(e.drop.data);

				if (fname_view.ends_with(".glb") || fname_view.ends_with(".vrm") ||
				        fname_view.ends_with(".GLB") || fname_view.ends_with(".VRM")) {
					uint32_t currentDropID = g_dropCounter.fetch_add(1, std::memory_order_relaxed);
					float mx, my;
					SDL_GetMouseState(&mx, &my);
					glm::vec4 viewport(0.0f, 0.0f, (float)mvkobjs.width, (float)mvkobjs.height);
					glm::vec3 nearPt = glm::unProject(glm::vec3(mx, viewport.w - my, 0.0f), persviewproj[0], persviewproj[1], viewport);
					glm::vec3 farPt  = glm::unProject(glm::vec3(mx, viewport.w - my, 1.0f), persviewproj[0], persviewproj[1], viewport);
					glm::vec3 rayDir = glm::normalize(farPt - nearPt);
					float t = (glm::abs(rayDir.y) > 0.001f) ? (0.0f - rview::core::camwpos.y) / rayDir.y : -1.0f;
					glm::vec3 hitPos = (t > 0.0f && t <= 50.0f) ? (rview::core::camwpos + rayDir * t) : (rview::core::camwpos + rayDir * 50.0f);
					vkrenderer::DropSession session{};
					session.dropID = currentDropID;
					session.spawnPos = hitPos;
					session.filename = std::string(fname_view);
					vkrenderer::g_activeDrops.emplace_back(std::move(session));
					vkrenderer::g_openDropModal = true;

					if (active_io_jobs.load(std::memory_order_acquire) == 0) g_job_strings.reset();

					const char* fname = g_job_strings.push_string(fname_view);

					g_jobs.enqueue([fname, currentDropID]() {
						model_manager::g_progress_queue.push({currentDropID, model_manager::ParseStep::parsing});


						model_manager::StagingModelData staging = model_manager::parse_model_to_staging(fname);

						model_manager::g_progress_queue.push({currentDropID, model_manager::ParseStep::done});

						staging.dropID = currentDropID;
						staging.skipui = false;

						active_io_jobs.fetch_add(1, std::memory_order_release);

						if (!staging.meshes.empty()) {
							while (!vkrenderer::pending_staging_models.push(std::move(staging))) {
								std::this_thread::yield();
							}
						}

						active_io_jobs.fetch_sub(1, std::memory_order_release);
					});

				}

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
// 1. MUST use && to avoid copy-constructor deleted errors
void vkrenderer::commitspawn(rvkbucket& mvkobjs, VkCommandBuffer c, model_manager::StagingModelData&& drop) {
	std::vector<uint32_t> uploadedTexIDs(drop.textures.size(), 0xFFFFFFFF);
	uint32_t modelID = model_manager::commit_staging_to_vulkan(mvkobjs, c, drop, uploadedTexIDs);

	for (uint32_t i = 0; i < drop.requested_instances; ++i) {
		uint32_t b_count = drop.parsed_skel ? drop.parsed_skel->nodeCount : 0;
		uint32_t j_count = drop.parsed_skel ? drop.parsed_skel->jointCount : 0;
		uint32_t b_start = (b_count > 0) ? static_cast<uint32_t>(model_manager::g_bone_locals.size()) : 0;
		uint32_t j_start = (j_count > 0) ? static_cast<uint32_t>(model_manager::g_joint_to_node.size()) : 0;

		model_manager::Entity new_ent = model_manager::g_registry.create_entity(
		                                    modelID, drop.isSkinned, b_start, b_count, j_start, j_count
		                                );

		if (!model_manager::g_registry.is_valid(new_ent)) continue;

		uint32_t dense_idx = model_manager::g_registry.get_dense_index(new_ent);

		if (b_count > 0) {
			for (uint32_t b = 0; b < b_count; ++b) {
				int32_t parent = drop.parsed_skel->parentIndices[b];
				model_manager::g_bone_parents.push_back(parent >= 0 ? parent + b_start : -1);
				model_manager::g_bone_entity_owner.push_back(dense_idx);
			}

			model_manager::g_bone_locals.insert(model_manager::g_bone_locals.end(),
			                                    drop.parsed_skel->localTransforms.get(), drop.parsed_skel->localTransforms.get() + b_count);
			model_manager::g_bone_globals.insert(model_manager::g_bone_globals.end(),
			                                     drop.parsed_skel->globalTransforms.get(), drop.parsed_skel->globalTransforms.get() + b_count);

			if (j_count > 0) {
				for (uint32_t j = 0; j < j_count; ++j) {
					model_manager::g_joint_to_node.push_back(drop.parsed_skel->jointToNodeMap[j] + b_start);
				}

				model_manager::g_joint_inverse_binds.insert(model_manager::g_joint_inverse_binds.end(),
				                                    drop.parsed_skel->inverseBindMatrices.get(), drop.parsed_skel->inverseBindMatrices.get() + j_count);

				for (uint32_t j = 0; j < j_count; ++j) {
					model_manager::g_joint_final_matrices.push_back(
					    Mat4ToDualQuatScale(drop.parsed_skel->finalJointMatrices[j])
					);
				}
			}
		}

		if (drop.demo_modifier) {
			drop.demo_modifier(i, drop.requested_instances, new_ent, drop.spawn_position);
		} else {
			model_manager::g_registry.position(new_ent) = drop.spawn_position;
		}
	}

	auto it = model_manager::g_cpuModels.find(modelID);

	if (it != model_manager::g_cpuModels.end()) {
		it->second.refCount += drop.requested_instances;
	}
}

void vkrenderer::spawnall(rvkbucket& mvkobjs, VkCommandBuffer c) {
	for (auto& drop : g_activeDrops) {
		drop.stagingData.requested_instances = drop.instanceCount;
		drop.stagingData.spawn_position = drop.spawnPos;
		commitspawn(mvkobjs, c, std::move(drop.stagingData));
	}

	g_activeDrops.clear();
}

void vkrenderer::cancelspawn(uint32_t id) {
	g_activeDrops.erase(g_activeDrops.begin() + id);
}

void vkrenderer::cancelall() {
	g_activeDrops.clear();
}

void vkrenderer::movecam(rvkbucket& mvkobjs) {
	static ImGuiIO* io = &ImGui::GetIO();
	float mx, my;
	const SDL_MouseButtonFlags mouseMask = SDL_GetMouseState(&mx, &my);
	const bool isRightClick = (mouseMask & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT));
	const bool isLeftClick  = (mouseMask & SDL_BUTTON_MASK(SDL_BUTTON_LEFT));
	float speed = 2.0f;
	rview::core::camfor   = (float(input_state[Key_W]) * speed) - (float(input_state[Key_S]) * speed);
	rview::core::camright = (float(input_state[Key_D]) * speed) - (float(input_state[Key_A]) * speed);
	rview::core::camup    = (float(input_state[Key_E]) * speed) - (float(input_state[Key_Q]) * speed);

	static bool wasLooking = false;
	auto* win = reinterpret_cast<SDL_Window*>(mvkobjs.wind);

	if (isRightClick != wasLooking) {
		if (isRightClick) {
			SDL_SetWindowRelativeMouseMode(win, true);

			float ignoreX, ignoreY;
			SDL_GetRelativeMouseState(&ignoreX, &ignoreY);
		} else {
			SDL_SetWindowRelativeMouseMode(win, false);

			SDL_WarpMouseInWindow(win, static_cast<float>(vkrenderer::mousex), static_cast<float>(vkrenderer::mousey));
		}

		wasLooking = isRightClick;
	}

	if (isRightClick) {
		float rx, ry;
		SDL_GetRelativeMouseState(&rx, &ry);
		rview::core::azimuth   += rx * 0.1f;
		rview::core::elevation -= ry * 0.1f;
		rview::core::elevation = std::clamp(rview::core::elevation, -89.0f, 89.0f);
	} else {
		vkrenderer::mousex = static_cast<int>(mx);
		vkrenderer::mousey = static_cast<int>(my);
	}

	persviewproj.at(0) = vkcam::getview();

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

				// TODO
				// modelsettings &s = mplayer[selectiondata.midx]
				//                    ->getinst(selectiondata.iidx)
				//                    ->getinstancesettings();
				// s.msworldpos = h;
				// playerlookto = glm::normalize(h - s.msworldpos);

				// if (glm::abs(h.x - s.msworldpos.x) > 2.1f || glm::abs(h.z - s.msworldpos.z) > 2.1f) {
				// 	s.msworldrot.y = glm::degrees(glm::atan(playerlookto.x, playerlookto.z));
				// }
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

void vkrenderer::sync_assets_to_gpu(VkCommandBuffer cmd, rvkbucket& mvkobjs) {
	if (!g_assets.requiresUpload.load(std::memory_order_acquire)) return;

	std::lock_guard<std::mutex> lock(g_assets.registryMutex);

	g_assets.requiresUpload.store(false, std::memory_order_release);

	bool descriptors_need_update = false;
	std::vector<VkBufferMemoryBarrier2> barriers;

	auto add_barrier = [&](VkBuffer buf) {
		if (buf == VK_NULL_HANDLE) return;

		VkBufferMemoryBarrier2 b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		b.buffer = buf;
		b.size = VK_WHOLE_SIZE;
		barriers.push_back(b);
	};

	uint32_t currentByteSize = static_cast<uint32_t>(g_assets.globalRawIndices.size());
	uint32_t newBytesToUpload = currentByteSize - rview::core::g_rawIndexOffset;

	if (newBytesToUpload > 0) {
		if (currentByteSize > rview::core::g_rawIndexCapacity) {
			uint32_t newCapacity = std::max(rview::core::g_rawIndexCapacity * 2, currentByteSize + (1024 * 1024 * 16));

			GpuBuffer newBuffer;
			vkvbo::init(mvkobjs, newBuffer, newCapacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			            VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			if (rview::core::g_rawIndexSSBO.buffer != VK_NULL_HANDLE) {
				VkBufferCopy copyRegion{0, 0, rview::core::g_rawIndexOffset};
				vkCmdCopyBuffer(cmd, rview::core::g_rawIndexSSBO.buffer, newBuffer.buffer, 1, &copyRegion);
			}

			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(newBytesToUpload);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.globalRawIndices.data() + rview::core::g_rawIndexOffset,
			            newBytesToUpload);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, newBytesToUpload);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_rawIndexOffset, newBytesToUpload};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, newBuffer.buffer, 1, &tailRegion);

			safe_cleanup(mvkobjs, rview::core::g_rawIndexSSBO);

			rview::core::g_rawIndexSSBO = newBuffer;
			rview::core::g_rawIndexCapacity = newCapacity;
			descriptors_need_update = true;
		} else {
			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(newBytesToUpload);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.globalRawIndices.data() + rview::core::g_rawIndexOffset,
			            newBytesToUpload);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, newBytesToUpload);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_rawIndexOffset, newBytesToUpload};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, rview::core::g_rawIndexSSBO.buffer, 1, &tailRegion);
		}

		rview::core::g_rawIndexOffset = currentByteSize;
		add_barrier(rview::core::g_rawIndexSSBO.buffer);
	}

	uint32_t currentMorphByteSize = static_cast<uint32_t>(g_assets.globalMorphBytes.size());
	uint32_t newMorphBytesToUpload = currentMorphByteSize - g_morphDeltaOffset;

	if (newMorphBytesToUpload > 0) {
		if (currentMorphByteSize > g_morphDeltaCapacity) {
			uint32_t newCapacity = std::max(g_morphDeltaCapacity * 2, currentMorphByteSize + (1024 * 1024 * 8));

			GpuBuffer newBuffer;
			vkvbo::init(mvkobjs, newBuffer, newCapacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			            VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			if (g_morphDeltaSSBO.buffer != VK_NULL_HANDLE) {
				VkBufferCopy copyRegion{0, 0, g_morphDeltaOffset};
				vkCmdCopyBuffer(cmd, g_morphDeltaSSBO.buffer, newBuffer.buffer, 1, &copyRegion);
			}

			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(newMorphBytesToUpload);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.globalMorphBytes.data() + g_morphDeltaOffset,
			            newMorphBytesToUpload);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, newMorphBytesToUpload);

			VkBufferCopy tailRegion{beltOffset, g_morphDeltaOffset, newMorphBytesToUpload};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, newBuffer.buffer, 1, &tailRegion);

			safe_cleanup(mvkobjs, g_morphDeltaSSBO);

			g_morphDeltaSSBO = newBuffer;
			g_morphDeltaCapacity = newCapacity;
			descriptors_need_update = true;
		} else {
			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(newMorphBytesToUpload);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.globalMorphBytes.data() + g_morphDeltaOffset,
			            newMorphBytesToUpload);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, newMorphBytesToUpload);

			VkBufferCopy tailRegion{beltOffset, g_morphDeltaOffset, newMorphBytesToUpload};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, g_morphDeltaSSBO.buffer, 1, &tailRegion);
		}

		g_morphDeltaOffset = currentMorphByteSize;
		add_barrier(g_morphDeltaSSBO.buffer);
	}

	uint32_t currentPrimCount = static_cast<uint32_t>(g_assets.primitives.size());
	uint32_t newPrims = currentPrimCount - rview::core::g_primOffset;

	if (newPrims > 0) {
		uint32_t uploadBytes = newPrims * sizeof(PrimitiveMetadata);

		if (currentPrimCount > rview::core::g_primCapacity) {
			uint32_t newCap = std::max(rview::core::g_primCapacity * 2, currentPrimCount + 10000);
			uint32_t newByteCap = newCap * sizeof(PrimitiveMetadata);

			GpuBuffer newBuffer;
			vkvbo::init(mvkobjs, newBuffer, newByteCap, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			if (rview::core::g_primitiveRegistrySSBO.buffer != VK_NULL_HANDLE) {
				VkBufferCopy copyRegion{0, 0, rview::core::g_primOffset * sizeof(PrimitiveMetadata)};
				vkCmdCopyBuffer(cmd, rview::core::g_primitiveRegistrySSBO.buffer, newBuffer.buffer, 1, &copyRegion);
			}

			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.primitives.data() + rview::core::g_primOffset, uploadBytes);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, uploadBytes);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_primOffset * sizeof(PrimitiveMetadata), uploadBytes};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, newBuffer.buffer, 1, &tailRegion);

			safe_cleanup(mvkobjs, rview::core::g_primitiveRegistrySSBO);
			rview::core::g_primitiveRegistrySSBO = newBuffer;
			rview::core::g_primCapacity = newCap;
			descriptors_need_update = true;
		} else {
			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.primitives.data() + rview::core::g_primOffset, uploadBytes);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, uploadBytes);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_primOffset * sizeof(PrimitiveMetadata), uploadBytes};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, rview::core::g_primitiveRegistrySSBO.buffer, 1, &tailRegion);
		}

		rview::core::g_primOffset = currentPrimCount;
		add_barrier(rview::core::g_primitiveRegistrySSBO.buffer);
	}

	uint32_t currentModelCount = static_cast<uint32_t>(g_assets.models.size());
	uint32_t newModels = currentModelCount - rview::core::g_modelOffset;

	if (newModels > 0) {
		uint32_t uploadBytes = newModels * sizeof(ModelMetadata);

		if (currentModelCount > rview::core::g_modelCapacity) {
			uint32_t newCap = std::max(rview::core::g_modelCapacity * 2, currentModelCount + 2000);
			uint32_t newByteCap = newCap * sizeof(ModelMetadata);

			GpuBuffer newBuffer;
			vkvbo::init(mvkobjs, newBuffer, newByteCap, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			if (rview::core::g_modelRegistrySSBO.buffer != VK_NULL_HANDLE) {
				VkBufferCopy copyRegion{0, 0, rview::core::g_modelOffset * sizeof(ModelMetadata)};
				vkCmdCopyBuffer(cmd, rview::core::g_modelRegistrySSBO.buffer, newBuffer.buffer, 1, &copyRegion);
			}

			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.models.data() + rview::core::g_modelOffset, uploadBytes);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, uploadBytes);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_modelOffset * sizeof(ModelMetadata), uploadBytes};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, newBuffer.buffer, 1, &tailRegion);

			safe_cleanup(mvkobjs, rview::core::g_modelRegistrySSBO);
			rview::core::g_modelRegistrySSBO = newBuffer;
			rview::core::g_modelCapacity = newCap;
			descriptors_need_update = true;
		} else {
			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.models.data() + rview::core::g_modelOffset, uploadBytes);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, uploadBytes);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_modelOffset * sizeof(ModelMetadata), uploadBytes};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, rview::core::g_modelRegistrySSBO.buffer, 1, &tailRegion);
		}

		rview::core::g_modelOffset = currentModelCount;
		add_barrier(rview::core::g_modelRegistrySSBO.buffer);
	}

	if (!barriers.empty()) {
		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
		dep.pBufferMemoryBarriers = barriers.data();
		vkCmdPipelineBarrier2(cmd, &dep);
	}

	if (descriptors_need_update) {
		vkDeviceWaitIdle(mvkobjs.vkdevice.device);
		playout::update_asset_descriptors(mvkobjs);
	}
}
bool vkrenderer::draw(rvkbucket& mvkobjs) {
	ZoneScoped;

	if (vkWaitForFences(mvkobjs.vkdevice.device, 1,
	                    &mvkobjs.fencez.at(rview::core::currentFrame), VK_TRUE,
	                    UINT64_MAX) != VK_SUCCESS) {
		return false;
	}

	// good spot as any
	mvkobjs.sbelts[rview::core::currentFrame].reset();

	if (vkResetFences(mvkobjs.vkdevice.device, 1,
	                  &mvkobjs.fencez.at(rview::core::currentFrame)) != VK_SUCCESS)
		return false;

	//temp
	mvkobjs.deletionQ.flush();

	auto& currentgraph = rview::core::frameGraphs[rview::core::currentFrame];
	currentgraph.clear();


	// mvkobjs.frameInstances.clear();


	VkCommandBuffer c = mvkobjs.cbuffers_graphics.at(0).at(rview::core::currentFrame);



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
	rview::core::tickdiff = tick - vkrenderer::mlasttick;
	rview::core::frametime = mframetimer.stop();
	mframetimer.start();

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(
	                   mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                   mvkobjs.semaphorez[0][rview::core::currentFrame], VK_NULL_HANDLE, &imgidx);

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
		sdlevent(mvkobjs, e);
	}


	// maybe this is not worth it...
	model_manager::ProgressUpdate update;

	while (model_manager::g_progress_queue.pop(update)) {
		for (auto& drop : g_activeDrops) {
			if (drop.dropID == update.requestID) {
				drop.currentStep = update.step;
				break;
			}
		}
	}

	model_manager::StagingModelData ephemeralstage;

	while (pending_staging_models.pop(ephemeralstage)) {
		if (ephemeralstage.skipui) {
			g_commit_queue.push_back(std::move(ephemeralstage));
		} else {
			for (auto& drop : g_activeDrops) {
				if (drop.dropID == ephemeralstage.dropID) {
					drop.stagingData = std::move(ephemeralstage);
					drop.parseFinished = true;
					break;
				}
			}
		}
	}

	//couldnt think of somewhere better to leave this
	for (auto& payload : g_commit_queue) {
		commitspawn(mvkobjs, c, std::move(payload));
	}

	g_commit_queue.clear();

	for (int i = (int)g_asset_death_row.size() - 1; i >= 0; --i) {
		auto& dead = g_asset_death_row[i];

		if (dead.framesRemaining == 0) {
			if (dead.vbo.buffer != VK_NULL_HANDLE) {
				vmaDestroyBuffer(mvkobjs.alloc, dead.vbo.buffer, dead.vbo.alloc);
			}

			for (auto& tex : dead.textures) {
				vmaDestroyImage(mvkobjs.alloc, tex.img, tex.alloc);
				vkDestroyImageView(mvkobjs.vkdevice.device, tex.imgview, nullptr);
			}

			// O(1) Remove
			g_asset_death_row[i] = std::move(g_asset_death_row.back());
			g_asset_death_row.pop_back();
		} else {
			dead.framesRemaining--;
		}
	}

	for (auto& kill : g_kill_queue) {
		model_manager::g_registry.destroy_entity(kill.entity);

		auto it = model_manager::g_cpuModels.find(kill.modelID);

		if (it != model_manager::g_cpuModels.end()) {

			it->second.refCount--;

			if (it->second.refCount == 0) {

				{
					std::lock_guard<std::mutex> lock(rview::core::global_materials.mtx);

					for (uint32_t matID : it->second.materialIDs) {
						rview::core::global_materials.free_slots.push(matID);
					}
				}

				vkrenderer::CondemnedAsset deadAsset;
				deadAsset.vbo = it->second.geometryVBO;
				deadAsset.textures = std::move(it->second.textures);

				g_asset_death_row.push_back(std::move(deadAsset));

				model_manager::g_cpuModels.erase(it);
			}
		}
	}

	g_kill_queue.clear();

	model_manager::update_logic_and_animations(rview::core::tickdiff);

	update_dynamic_instances(mvkobjs);

	sync_assets_to_gpu(c, mvkobjs);

	movecam(mvkobjs);

	moveplayer();

	{
		// invisible lock
		void* data;
		vmaMapMemory(mvkobjs.alloc, rview::core::globalCameraUBO.alloc, &data);
		std::byte* dst = static_cast<std::byte*>(data);
		std::memcpy(dst, persviewproj.data(), 2 * sizeof(glm::mat4));
		glm::vec4 campos4(rview::core::camwpos, 0.0f);
		std::memcpy(dst + (2 * sizeof(glm::mat4)), &campos4, sizeof(glm::vec4));
		vmaUnmapMemory(mvkobjs.alloc, rview::core::globalCameraUBO.alloc);
	}
	model_manager::upload_joint_matrices(mvkobjs);


	std::call_once(obswrite, [&]() {
		vkrenderer::write_obs(mvkobjs);
	});



	// let that sync in
	currentgraph.add_pass([&mvkobjs, c, imgidx] {

		// VkBufferMemoryBarrier2 indirectBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		// indirectBarrier.buffer = mvkobjs.globalIndirectBuffers[rview::core::currentFrame].buffer;
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

		VkBufferMemoryBarrier2 bufferBarriers[] = {}; // just idling
		VkImageMemoryBarrier2 imageBarriers[] = {imageBarrier, depthBarrier};

		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.bufferMemoryBarrierCount = 0;
		dep.pBufferMemoryBarriers = bufferBarriers;
		dep.imageMemoryBarrierCount = 2;
		dep.pImageMemoryBarriers = imageBarriers;

		vkCmdPipelineBarrier2(c, &dep);
	});

	currentgraph.add_pass([c] {

		vkCmdFillBuffer(c, rview::core::g_indirectCountBuffers[rview::core::currentFrame].buffer, 0, sizeof(uint32_t), 0);

		VkBufferMemoryBarrier2 fillBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		fillBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		fillBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		fillBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		fillBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		fillBarrier.buffer = rview::core::g_indirectCountBuffers[rview::core::currentFrame].buffer;
		fillBarrier.size = sizeof(uint32_t);

		VkDependencyInfo fillDep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		fillDep.bufferMemoryBarrierCount = 1;
		fillDep.pBufferMemoryBarriers = &fillBarrier;
		vkCmdPipelineBarrier2(c, &fillDep);

		uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

		if (active_instances == 0) return;

		vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_COMPUTE, rview::core::globalcullpline);
		vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_COMPUTE, rview::core::globalPipelineLayout, 0, 1, &rview::core::globalBindlessSet, 0, nullptr);

		vkpushconstants pc{};
		pc.stride = active_instances;
		pc.frameIndex = rview::core::currentFrame;
		vkCmdPushConstants(c, rview::core::globalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vkpushconstants), &pc);

		vkCmdDispatch(c, (active_instances + 63) / 64, 1, 1);

		VkBufferMemoryBarrier2 computeBarriers[2] = {};
		computeBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		computeBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		computeBarriers[0].srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		computeBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
		computeBarriers[0].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
		computeBarriers[0].buffer = rview::core::g_indirectCommandBuffers[rview::core::currentFrame].buffer;
		computeBarriers[0].size = VK_WHOLE_SIZE;

		computeBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		computeBarriers[1].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		computeBarriers[1].srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		computeBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
		computeBarriers[1].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
		computeBarriers[1].buffer = rview::core::g_indirectCountBuffers[rview::core::currentFrame].buffer;
		computeBarriers[1].size = sizeof(uint32_t);

		VkDependencyInfo computeDep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		computeDep.bufferMemoryBarrierCount = 2;
		computeDep.pBufferMemoryBarriers = computeBarriers;
		vkCmdPipelineBarrier2(c, &computeDep);
	});

	currentgraph.add_pass([&mvkobjs, c, imgidx] {


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
		color_attach.clearValue = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
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



		vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, playout::skinnedpline);
		vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_GRAPHICS, rview::core::globalPipelineLayout, 0, 1, &rview::core::globalBindlessSet, 0, nullptr);

		vkpushconstants pc_mdi{};
		pc_mdi.frameIndex = rview::core::currentFrame;
		vkCmdPushConstants(c, rview::core::globalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vkpushconstants), &pc_mdi);

		vkCmdDrawIndirectCount(
		    c,
		    rview::core::g_indirectCommandBuffers[rview::core::currentFrame].buffer, 0,
		    rview::core::g_indirectCountBuffers[rview::core::currentFrame].buffer, 0,
		    100000, sizeof(VkDrawIndirectCommand));

		// TODO MOVE to separate async pipeline >:( i probably wont ever but eh.. the thought that counts ig
		ui::createdbgframe(mvkobjs);
		ui::createdropwidget(mvkobjs, c);
		ui::render(mvkobjs, c);

		vkCmdEndRendering(c);

	});


	currentgraph.add_pass([&mvkobjs, c, imgidx] {

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

	vkrenderer::copy_engine_to_obs(c, mvkobjs, mvkobjs.schainimgs.at(imgidx), mvkobjs.obs_target.img, mvkobjs.obs_target.width,
	                               mvkobjs.obs_target.height);


	if (vkEndCommandBuffer(c) != VK_SUCCESS)
		return false;


	// wtf
	muigentimer.start();
	rview::core::rduigeneratetime = muigentimer.stop();





	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::array<VkPipelineStageFlags, 1> waitstage = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		// ,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
	};
	submitinfo.pWaitDstStageMask = waitstage.data();
	std::array<VkSemaphore, 1> waitsemas{
		mvkobjs.semaphorez.at(0).at(rview::core::currentFrame)
		                  //   ,mvkobjs.semaphorez.at(2).at(rview::core::currentFrame)
	};
	submitinfo.waitSemaphoreCount = waitsemas.size();
	submitinfo.pWaitSemaphores = waitsemas.data();

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.semaphorez.at(1).at(imgidx);

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &c;

	vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.fencez.at(rview::core::currentFrame));

	{
		std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
		auto res  = vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo,
		                          mvkobjs.fencez.at(rview::core::currentFrame));

		//   vkQueueWaitIdle(mvkobjs.graphicsQ);
		if (res != VK_SUCCESS) {
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

	rview::core::mtx2->lock();
	res = vkQueuePresentKHR(mvkobjs.presentQ, &presentinfo);

	rview::core::mtx2->unlock();

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain(mvkobjs);
	} else {
		if (res != VK_SUCCESS) {
			return false;
		}
	}

	obs_bridge::send_spout();

	vkrenderer::mlasttick = tick;
	rview::core::currentFrame =
	    (rview::core::currentFrame + 1) % rview::core::MAX_FRAMES_IN_FLIGHT;

	return true;
}
