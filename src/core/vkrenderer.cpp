#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION
#include <backends/imgui_impl_sdl3.h>
#include <core/rvk.hpp>
#include <cstdint>
#include <vkrenderer.hpp>
#include <imgui.h>
#include <vk_mem_alloc.h>
#include <cstdlib>
#include <ctime>
#include <future>
#include <iostream>
#include <ranges>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/spline.hpp>
#include <dbg/demo.hpp>
#include <vk/vkstuff.hpp>
#include <vksyncobjects.hpp>
#include <SDL3/SDL_vulkan.h>
#include <vktex.hpp>
#include <playout.hpp>
#include <ubo.hpp>
#include <obs_bridge.hpp>

// i ccba
inline void jdump() noexcept {
	size_t hq = gjobs.hpqueue.eqpos.load() - gjobs.hpqueue.dqpos.load();
	size_t lq = gjobs.lpqueue.eqpos.load() - gjobs.lpqueue.dqpos.load();

	SDL_Log("jobs: queued high %zu low %zu | parked general %d high-only %d of %zu workers | loads in flight %d | full hits %llu",
		hq, lq, gjobs.nidle.load(), gjobs.hnidle.load(), gjobs.workers.size(), gaiojobs.load(),
		static_cast<unsigned long long>(gjobs.nfull.load()));
}

void vkrenderer::immediate_submit(rvkbucket& mvkobjs, std::function<void(VkCommandBuffer cbuffer)>&& fn) {
	VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = mvkobjs.cpools_graphics.at(0);
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cbuffer;
	vkAllocateCommandBuffers(mvkobjs.vkdevice.device, &allocInfo, &cbuffer);

	VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
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

	vkFreeCommandBuffers(mvkobjs.vkdevice.device, mvkobjs.cpools_graphics.at(0), 1, &cbuffer);
}

// gotta move these somewhere more reasonable someday
bool init_global_samplers(rvkbucket& objs) {
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

	if (vkCreateSampler(objs.vkdevice.device, &info, nullptr, &objs.samplerz[0]) != VK_SUCCESS) {
		return false;
	}

	gexitq.enqueue(tdphase::device,
					[device = objs.vkdevice.device, sampler = objs.samplerz[0]]() { vkDestroySampler(device, sampler, nullptr); });


	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if (vkCreateSampler(objs.vkdevice.device, &info, nullptr, &objs.samplerz[1]) != VK_SUCCESS) {
		return false;
	}

	gexitq.enqueue(tdphase::device,
					[device = objs.vkdevice.device, sampler = objs.samplerz[1]]() { vkDestroySampler(device, sampler, nullptr); });

	return true;
}

bool init_dummy_textures(rvkbucket& objs, VkCommandPool& cmdPool) {

	auto find_memory_type = [&](uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(objs.vkdevice.physical_device, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		return 0;
	};
	auto create_buffer =
		[&](VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
			VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			vkCreateBuffer(objs.vkdevice.device, &bufferInfo, nullptr, &buffer);

			VkMemoryRequirements memReqs;
			vkGetBufferMemoryRequirements(objs.vkdevice.device, buffer, &memReqs);

			VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
			allocInfo.allocationSize = memReqs.size;
			allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, properties);

			vkAllocateMemory(objs.vkdevice.device, &allocInfo, nullptr, &bufferMemory);
			vkBindBufferMemory(objs.vkdevice.device, buffer, bufferMemory, 0);
		};

	auto record_barrier = [](VkCommandBuffer cmd,
							 VkImage image,
							 VkImageLayout oldLayout,
							 VkImageLayout newLayout,
							 VkAccessFlags srcAccess,
							 VkAccessFlags dstAccess,
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

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
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
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		vkCreateImage(objs.vkdevice.device, &imageInfo, nullptr, &tex.image);

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(objs.vkdevice.device, tex.image, &memReqs);

		VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = find_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

		create_buffer(imageSize,
					  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					  stagingBuf,
					  stagingMem);

		// Map & Copy Data
		void* data;
		vkMapMemory(objs.vkdevice.device, stagingMem, 0, imageSize, 0, &data);
		memcpy(data, &pixelData, (size_t)imageSize);
		vkUnmapMemory(objs.vkdevice.device, stagingMem);

		vkrenderer::immediate_submit(objs, [&](VkCommandBuffer cmd) {
			record_barrier(cmd,
						   tex.image,
						   VK_IMAGE_LAYOUT_UNDEFINED,
						   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   0,
						   VK_ACCESS_TRANSFER_WRITE_BIT,
						   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						   VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			region.imageOffset = {0, 0, 0};
			region.imageExtent = {1, 1, 1};

			vkCmdCopyBufferToImage(cmd, stagingBuf, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			record_barrier(cmd,
						   tex.image,
						   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						   VK_ACCESS_TRANSFER_WRITE_BIT,
						   VK_ACCESS_SHADER_READ_BIT,
						   VK_PIPELINE_STAGE_TRANSFER_BIT,
						   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		});

		vkDestroyBuffer(objs.vkdevice.device, stagingBuf, nullptr);
		vkFreeMemory(objs.vkdevice.device, stagingMem, nullptr);

		gexitq.enqueue(tdphase::device, [device = objs.vkdevice.device, img = tex.image, view = tex.view, mem = tex.memory]() {
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
	rview::core::globalTextureCounter.store(4, std::memory_order_release);

	return true;
}
float map2(glm::vec3 x) {
	return std::max(x.y, 0.0f);
}


static VkQueryPool g_gpuQueryPool = VK_NULL_HANDLE;
static float g_gpuTimestampPeriod = 0.0f;
static bool g_gpuQueryUsed[rview::core::MAX_FRAMES_IN_FLIGHT] = {};
static uint64_t g_lastVmemSample = 0;


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

	if (!initvma(mvkobjs))
		return false;

	if (!getqueue(mvkobjs))
		return false;

	{
		VmaAllocatorInfo allocInfo{};
		vmaGetAllocatorInfo(mvkobjs.alloc, &allocInfo);
		const VkPhysicalDeviceProperties* physProps = nullptr;
		vmaGetPhysicalDeviceProperties(mvkobjs.alloc, &physProps);

		uint32_t qfCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(allocInfo.physicalDevice, &qfCount, nullptr);
		std::vector<VkQueueFamilyProperties> qfProps(qfCount);
		if (qfCount > 0) {
			vkGetPhysicalDeviceQueueFamilyProperties(allocInfo.physicalDevice, &qfCount, qfProps.data());
		}

		const float period = physProps ? physProps->limits.timestampPeriod : 0.0f;
		const uint32_t validBits = (mvkobjs.graphicsQidx < qfCount) ? qfProps[mvkobjs.graphicsQidx].timestampValidBits : 0u;

		if (period > 0.0f && validBits > 0u) {
			VkQueryPoolCreateInfo qpci{};
			qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
			qpci.queryCount = static_cast<uint32_t>(2 * rview::core::MAX_FRAMES_IN_FLIGHT);
			if (vkCreateQueryPool(mvkobjs.vkdevice.device, &qpci, nullptr, &g_gpuQueryPool) == VK_SUCCESS) {
				g_gpuTimestampPeriod = period;
			} else {
				g_gpuQueryPool = VK_NULL_HANDLE;
			}
		}
	}

	if (!createswapchain(mvkobjs))
		return false;

	if (!createdepthbuffer(mvkobjs))
		return false;

	if (!createcommandpool(mvkobjs))
		return false;

	if (!createcommandbuffer(mvkobjs))
		return false;

	if (!createsyncobjects(mvkobjs))
		return false;

	if (!initcpuQs(mvkobjs))
		return false;

	model_manager::reserve_mega_buffers(rview::anim::megabuffers);

	if (!playout::init_bindless(mvkobjs))
		return false;

	if (!initglobalmats(mvkobjs))
		return false;

	model_manager::init_gpu_joint_buffers(mvkobjs);

	if (!playout::init_pipelines(mvkobjs))
		return false;

	if (!init_global_samplers(mvkobjs))
		return false;

	if (!init_dummy_textures(mvkobjs, mvkobjs.cpools_graphics.at(0)))
		return false;

	if (!rview::rvk::tex::load_env_map(mvkobjs, rview::core::exrtex.at(0)))
		return false;

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);

	if (!ubo::init_global(mvkobjs))
		return false;

	if (!initglobalinstances(mvkobjs))
		return false;

	if (!initglobalmorphs(mvkobjs))
		return false;

	if (!initglobalindirect(mvkobjs))
		return false;

	if (!initglobalidrectUNINDEXED(mvkobjs))
		return false;

	if (!create_obs_target(mvkobjs))
		return false;

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);




	mvkobjs.height = mvkobjs.schain.extent.height;
	mvkobjs.width = mvkobjs.schain.extent.width;

	if (initui(mvkobjs)) {
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	} else
		return false;

	mframetimer.start();

	return true;
}
bool vkrenderer::initcpuQs(rvkbucket& mvkobjs) {
	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; ++i) {
		if constexpr (rview::core::platform::is_android) {
			if (!mvkobjs.sbelts[i].init(mvkobjs.alloc, 32 * 1024 * 1024))
				return false;
		} else {
			if (!mvkobjs.sbelts[i].init(mvkobjs.alloc, 256 * 1024 * 1024))
				return false;
		}
	}

	// only 4
	for (uint32_t i = 0; i < 4; ++i) {
		StagingBelt* workerBelt = new StagingBelt();
		if (!workerBelt->init(mvkobjs.alloc, 128 * 1024 * 1024))
			return false;

		while (!g_worker_belts.push(workerBelt)) {
			rview::core::cpu_relax();
		}
	}

	return true;
}
bool vkrenderer::initscene(rvkbucket& mvkobjs) {

	if constexpr (rdebug::is_active || rdebug::has_tracy) {
		rdebug::init(mvkobjs.inst, mvkobjs.vkdevice.physical_device, mvkobjs.vkdevice, mvkobjs.graphicsQ, mvkobjs.cbuffers_graphics.at(0).at(0));
	}

	if constexpr (rdemo::is_active) {
		const auto& scene = rdemo::SCENES[1];

		for (const auto& spawn : scene.spawns) {
			const char* fname = gjstrings.pstring(spawn.filepath);
			uint32_t count = spawn.count;
			glm::vec3 origin = spawn.origin;
			auto modifier = spawn.modifier;

			const uint32_t lid = gm.lnew(fname);
			gaiojobs.fetch_add(1, std::memory_order_release);
			gjobs.enqueue(
				[fname, count, origin, modifier, lid, &mvkobjs]() noexcept {
					SDL_Log("[load %u] job started", 999999);
					sjguard ioguard(gaiojobs);
					mloadscope mls(lid);
					ZoneScopedN("load");

					try {

						model_manager::StagingModelData staging = model_manager::parse_model_to_staging(fname);
						gm.lstep(mstep::parsed);
						staging.loadID = lid;
						SDL_Log("[load %u] parsed: %zu meshes, %zu textures", 999999, staging.meshes.size(), staging.textures.size());
						staging.requested_instances = count;
						staging.spawn_position = origin;
						staging.skipui = true;
						staging.demo_modifier = modifier;

						if (!staging.meshes.empty()) {
							std::vector<uint32_t> uploadedTexIDs(staging.textures.size(), 0xFFFFFFFF);

							staging.assignedModelID = model_manager::commit_staging_to_vulkan(mvkobjs, staging, uploadedTexIDs);
							gm.lstep(mstep::committed);
							SDL_Log("[load %u] committed as model %u", 999999, staging.assignedModelID);
							auto it = model_manager::g_cpuModels.find(staging.assignedModelID);
							staging.defaultPool = (it != model_manager::g_cpuModels.end() && !it->second.defaultClips.empty())
													  ? it->second.defaultClips[0]
													  : model_manager::FALLBACK_POOL_HANDLE;

							while (!vkrenderer::pending_staging_models.push(std::move(staging))) {
								if (!gjobs.isrun())
									break;
								rview::core::cpu_relax();
							}
							gm.lstep(mstep::pushed);
							SDL_Log("[load %u] handed to main thread", 999999);

						} else {
							std::cerr << "Failed to find demo models: " << fname << "\n";
						}
					} catch (const std::exception& e) {
						std::cerr << "Failed to load " << fname << ": " << e.what() << "\n";
					}
				},
				jprio::low);
		}

		return true; // will skip boot.json for nows
	}

	simdjson::ondemand::parser p;
	simdjson::padded_string j;

	if constexpr (rview::core::platform::is_android) {
		size_t datasize = 0;
		void* data = SDL_LoadFile("boot.json", &datasize);
		if (!data) {
			std::cerr << "X_X Failed to load boot.json via SDL" << std::endl;
			return false;
		}
		j = simdjson::padded_string(static_cast<char*>(data), datasize);
		SDL_free(data);
	} else {
		if (auto error = simdjson::padded_string::load("boot.json").get(j); error) {
			std::cerr << "X_X " << error << std::endl;
			return false;
		}
	}

	simdjson::ondemand::document d;

	if (auto error = p.iterate(j).get(d); error)
		return false;

	simdjson::ondemand::array models;

	if (d["models"].get(models))
		return false;

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
		const char* fname = gjstrings.pstring(f);
		gaiojobs.fetch_add(1, std::memory_order_release);

		gjobs.enqueue(
			[fname, instance_count, start_pos, &mvkobjs]() noexcept {
				sjguard ioguard(gaiojobs);

				// quitting: sjoin() still runs every queued job, so a load that has not started has to end here
				if (!gjobs.isrun())
					return;

				// jobs are noexcept: a throw from parsing or uploading has to end here
				try {
					model_manager::StagingModelData staging = model_manager::parse_model_to_staging(fname);

					// quitting: drop the model before any of it reaches the gpu
					if (!gjobs.isrun())
						return;

					staging.requested_instances = instance_count;
					staging.dropID = 0xFFFFFFFF; // irrelevant
					staging.skipui = true;
					staging.spawn_position = start_pos;

					if (!staging.meshes.empty()) {
						std::vector<uint32_t> uploadedTexIDs(staging.textures.size(), 0xFFFFFFFF);

						staging.assignedModelID = model_manager::commit_staging_to_vulkan(mvkobjs, staging, uploadedTexIDs);

						auto it = model_manager::g_cpuModels.find(staging.assignedModelID);
						staging.defaultPool = (it != model_manager::g_cpuModels.end() && !it->second.defaultClips.empty())
												  ? it->second.defaultClips[0]
												  : model_manager::FALLBACK_POOL_HANDLE;

						while (!vkrenderer::pending_staging_models.push(std::move(staging))) {
							if (!gjobs.isrun())
								break;
							rview::core::cpu_relax();
						}

					} else {
						std::cerr << "Failed to parse boot model: " << fname << "\n";
					}
				} catch (const std::exception& e) {
					std::cerr << "Failed to load " << fname << ": " << e.what() << "\n";
				}
			},
			jprio::low);
	}

	return true;
}
bool vkrenderer::initglobalmats(rvkbucket& mvkobjs) {
	size_t bufferSize = rview::core::MAX_GLOBAL_MATERIALS * sizeof(MaterialData);
	VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	binfo.size = bufferSize;
	binfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VmaAllocationCreateInfo vmaallocinfo{};
	vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	vmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocResult;

	if (vmaCreateBuffer(
			mvkobjs.alloc, &binfo, &vmaallocinfo, &rview::core::global_materials.buffer, &rview::core::global_materials.alloc, &allocResult) !=
		VK_SUCCESS) {
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

	VkDescriptorBufferInfo ssboInfo{rview::core::global_materials.buffer, 0, bufferSize};
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
		ivmaallocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // VMA_MEMORY_USAGE_CPU_TO_GPU
		// ivmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (vmaCreateBuffer(mvkobjs.alloc,
							&ibinfo,
							&ivmaallocinfo,
							&rview::core::globalInstanceBuffers[i].buffer,
							&rview::core::globalInstanceBuffers[i].alloc,
							nullptr) != VK_SUCCESS) {
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

		VkBufferCreateInfo stateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		stateInfo.size = sizeof(GPUEntityState) * SceneData::MAX_ENTITIES;
		stateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		VmaAllocationCreateInfo stateAllocInfo{};
		stateAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		stateAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (vmaCreateBuffer(mvkobjs.alloc,
							&stateInfo,
							&stateAllocInfo,
							&rview::core::globalEntityStateBuffers[i].buffer,
							&rview::core::globalEntityStateBuffers[i].alloc,
							nullptr) != VK_SUCCESS) {
			return false;
		}

		VkDescriptorBufferInfo stateSboInfo{rview::core::globalEntityStateBuffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet stateWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		stateWrite.dstSet = rview::core::globalBindlessSet;
		stateWrite.dstBinding = 20;
		stateWrite.dstArrayElement = i;
		stateWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		stateWrite.descriptorCount = 1;
		stateWrite.pBufferInfo = &stateSboInfo;

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &stateWrite, 0, nullptr);
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

		if (vmaCreateBuffer(mvkobjs.alloc,
							&ibinfo,
							&ivmaallocinfo,
							&rview::core::globalIndirectBuffers[i].buffer,
							&rview::core::globalIndirectBuffers[i].alloc,
							nullptr) != VK_SUCCESS) {
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
	uint32_t maxDrawCommands = rview::core::platform::is_android ? 10000 : 100000;

	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; i++) {

		if (!vkvbo::init(mvkobjs,
						 rview::core::g_indirectCommandBuffers[i],
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
	}


	if (!vkvbo::init(mvkobjs,
					 rview::core::g_indirectCountBuffers,
					 sizeof(uint32_t) * rview::core::MAX_FRAMES_IN_FLIGHT,
					 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
		return false;
	}

	VkDescriptorBufferInfo countInfo{rview::core::g_indirectCountBuffers.buffer, 0, VK_WHOLE_SIZE};
	VkWriteDescriptorSet countWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	countWrite.dstSet = rview::core::globalBindlessSet;
	countWrite.dstBinding = 16;
	countWrite.dstArrayElement = 0; // The driver can't clamp this, it's just 0!
	countWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	countWrite.descriptorCount = 1;
	countWrite.pBufferInfo = &countInfo;
	vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &countWrite, 0, nullptr);

	return true;
}
void vkrenderer::update_dynamic_instances(rvkbucket& mvkobjs, uint32_t read_frame) {
	uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

	if (active_instances == 0)
		return;

	uint32_t safe_instances = std::min(active_instances, 65535U);

	uint32_t max_model_id = static_cast<uint32_t>(g_assets.models.size());
	static std::vector<uint8_t> model_skin_cache;
	model_skin_cache.assign(max_model_id, 0);

	for (const auto& [id, cpuModel] : model_manager::g_cpuModels) {
		if (id < max_model_id)
			model_skin_cache[id] = cpuModel.isSkinned;
	}

	// void* mappedData;
	// vmaMapMemory(mvkobjs.alloc, rview::core::globalInstanceBuffers[rview::core::currentFrame].alloc, &mappedData);
	// GPUInstanceData* gpuInsts = static_cast<GPUInstanceData*>(mappedData);
	void* mappedData;
	vmaMapMemory(mvkobjs.alloc, rview::core::globalEntityStateBuffers[rview::core::currentFrame].alloc, &mappedData);
	GPUEntityState* gpuEntities = static_cast<GPUEntityState*>(mappedData);

	void* weightData;
	vmaMapMemory(mvkobjs.alloc, g_morphWeightBuffers[rview::core::currentFrame].alloc, &weightData);
	float* mappedWeights = static_cast<float*>(weightData);

	for (uint32_t i = 0; i < safe_instances; ++i) {
		if (!model_manager::g_registry.is_visible[i] || g_scene.modelIDs[i] == 0xFFFFFFFF) {
			gpuEntities[i].modelID = 0xFFFFFFFF;
			continue;
		}

		gpuEntities[i].position = glm::vec4(g_scene.worldPositions[i], 1.0f);
		gpuEntities[i].rotation = g_scene.rotations[i];
		gpuEntities[i].scale = glm::vec4(g_scene.scales[i], 1.0f);
		gpuEntities[i].modelID = g_scene.modelIDs[i];
		gpuEntities[i].morphWeightOffset = model_manager::g_registry.morph_weight_offsets[i];

		uint32_t poolHandle = g_scene.poolHandles[i];
		const auto* pool = model_manager::g_animPoolRegistry.GetPool(poolHandle);
		bool hasSkin = (g_scene.modelIDs[i] < max_model_id) ? model_skin_cache[g_scene.modelIDs[i]] : false;

		if (!hasSkin) {
			gpuEntities[i].isSkinned = 0;
			gpuEntities[i].jointOffset = 0;
		} else if ((pool->pipelineFlags & model_manager::ANIM_FLAG_GPU_BAKED) != 0 && pool->bakedBindlessID != 0xFFFFFFFF) {
			gpuEntities[i].isSkinned = 2 | (pool->bakedBindlessID << 16);
			gpuEntities[i].jointOffset = pool->currentFrameIndex[read_frame] * pool->jointCount;
		} else {
			gpuEntities[i].isSkinned = 1;
			gpuEntities[i].jointOffset = pool->globalJointOffset;
		}
	}

	size_t total_morph_bytes = model_manager::g_registry.flat_morph_weights.size() * sizeof(float);
	if (total_morph_bytes > 0) {
		std::memcpy(mappedWeights, model_manager::g_registry.flat_morph_weights.data(), total_morph_bytes);
	}

	vmaFlushAllocation(
		mvkobjs.alloc, rview::core::globalEntityStateBuffers[rview::core::currentFrame].alloc, 0, safe_instances * sizeof(GPUEntityState));
	vmaUnmapMemory(mvkobjs.alloc, rview::core::globalEntityStateBuffers[rview::core::currentFrame].alloc);

	vmaFlushAllocation(mvkobjs.alloc, g_morphWeightBuffers[rview::core::currentFrame].alloc, 0, total_morph_bytes);
	vmaUnmapMemory(mvkobjs.alloc, g_morphWeightBuffers[rview::core::currentFrame].alloc);
}
bool vkrenderer::initvma(rvkbucket& mvkobjs) {
	VmaAllocatorCreateInfo allocinfo{};
	allocinfo.vulkanApiVersion = rview::core::platform::is_android ? VK_API_VERSION_1_3 : VK_API_VERSION_1_4;
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
	mvkobjs.graphicsQ = mvkobjs.vkdevice.get_queue(vkb::QueueType::graphics).value();
	mvkobjs.graphicsQidx = mvkobjs.vkdevice.get_queue_index(vkb::QueueType::graphics).value();
	mvkobjs.presentQ = mvkobjs.vkdevice.get_queue(vkb::QueueType::present).value();
	mvkobjs.presentQidx = mvkobjs.vkdevice.get_queue_index(vkb::QueueType::present).value();
	mvkobjs.computeQ = mvkobjs.vkdevice.get_queue(vkb::QueueType::compute).value();
	mvkobjs.computeQidx = mvkobjs.vkdevice.get_queue_index(vkb::QueueType::compute).value();
	auto transfer_queue_ret = mvkobjs.vkdevice.get_queue(vkb::QueueType::transfer);

	if (transfer_queue_ret.has_value()) {
		mvkobjs.transferQ = transfer_queue_ret.value();
		mvkobjs.transferQidx = mvkobjs.vkdevice.get_queue_index(vkb::QueueType::transfer).value();
	} else {
		mvkobjs.transferQ = mvkobjs.vkdevice.get_queue(vkb::QueueType::graphics).value();
		mvkobjs.transferQidx = mvkobjs.vkdevice.get_queue_index(vkb::QueueType::graphics).value();
	}

	return true;
}
bool vkrenderer::createdepthbuffer(rvkbucket& mvkobjs) {
	VkExtent3D depthimageextent = {mvkobjs.schain.extent.width, mvkobjs.schain.extent.height, 1};

	if constexpr (rview::core::platform::is_android) {
		VkFormat depth_formats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
		mvkobjs.rddepthformat = VK_FORMAT_UNDEFINED;
		for (auto& format : depth_formats) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(mvkobjs.vkdevice.physical_device, format, &props);
			if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				mvkobjs.rddepthformat = format;
				break;
			}
		}
		if (mvkobjs.rddepthformat == VK_FORMAT_UNDEFINED)
			return false;
	} else {
		mvkobjs.rddepthformat = VK_FORMAT_D32_SFLOAT;
	}

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
	depthallocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vmaCreateImage(mvkobjs.alloc, &depthimginfo, &depthallocinfo, &mvkobjs.rddepthimage, &mvkobjs.rddepthimagealloc, nullptr) != VK_SUCCESS) {
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

	if (vkCreateImageView(mvkobjs.vkdevice, &depthimgviewinfo, nullptr, &mvkobjs.rddepthimageview) != VK_SUCCESS) {
		return false;
	}

	return true;
}
bool vkrenderer::createswapchain(rvkbucket& mvkobjs) {
	if constexpr (rview::core::platform::is_android) {
		if (!mvkobjs.surface)
			return true;
	}
	vkb::SwapchainBuilder swapchainbuild{mvkobjs.vkdevice};

	VkSurfaceCapabilitiesKHR surcap;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.vkdevice.physical_device, mvkobjs.surface, &surcap);

	swapchainbuild.set_composite_alpha_flags((VkCompositeAlphaFlagBitsKHR)(surcap.supportedCompositeAlpha & 8 ? 8 : 1));
	if constexpr (rview::core::platform::is_android) {
		swapchainbuild.use_default_format_selection();
	} else {
		swapchainbuild.set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
	}

	auto swapchainbuilret = swapchainbuild.set_old_swapchain(mvkobjs.schain).set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR).build();
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
	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);

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
	if (!commandpool::createsametype(
			mvkobjs, mvkobjs.cpools_transfer, rview::core::platform::is_android ? vkb::QueueType::graphics : vkb::QueueType::transfer)) [[unlikely]] {
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

		if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo, &g_morphWeightBuffers[i].buffer, &g_morphWeightBuffers[i].alloc, nullptr) !=
			VK_SUCCESS) {
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

	if constexpr (rview::core::platform::is_windows) {
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
							 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0,
							 0,
							 nullptr,
							 0,
							 nullptr,
							 1,
							 &barrier);

		VkClearColorValue clearColor = {{1.0f, 0.0f, 1.0f, 1.0f}};
		VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		vkCmdClearColorImage(mvkobjs.cbuffers_graphics.at(0).at(rview::core::currentFrame),
							 mvkobjs.obs_target.img,
							 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							 &clearColor,
							 1,
							 &range);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

		vkCmdPipelineBarrier(mvkobjs.cbuffers_graphics.at(0).at(rview::core::currentFrame),
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							 0,
							 0,
							 nullptr,
							 0,
							 nullptr,
							 1,
							 &barrier);
	}
}
void vkrenderer::cleanup(rvkbucket& mvkobjs) {

	cancelall();

	while (gaiojobs.load(std::memory_order_acquire) > 0) {
		SDL_Event e;

		while (SDL_PollEvent(&e)) {
			ImGui_ImplSDL3_ProcessEvent(&e); // main thread NEVER help.
		}

		// gjobs.hexec(); // idk if this was a good idea..
		rview::core::cpu_relax();
	}

	gjobs.sjoin();

	rview::io::save_state_to_json();

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);

	if constexpr (rdebug::has_tracy) {
		TracyVkDestroy(rdebug::tracyCTX);
	}

	ui::cleanup(mvkobjs);
	vksyncobjects::cleanup(mvkobjs);

	for (auto& x : mvkobjs.dpools)
		rpool::destroy(mvkobjs.vkdevice.device, x);

	playout::cleanup_pipelines(mvkobjs);

	model_manager::StagingModelData temp;

	while (pending_staging_models.pop(temp)) {}

	model_manager::OrphanedAnimBuffer orphaned_anim;

	while (model_manager::g_orphanedAnimBuffers.pop(orphaned_anim)) {
		if (orphaned_anim.vbo.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(mvkobjs.alloc, orphaned_anim.vbo.buffer, orphaned_anim.vbo.alloc);
		}
	}

	//temp
	mvkobjs.cleanupQ.flush();
	mvkobjs.deletionQ.flush();

	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);


	if constexpr (rview::core::platform::is_windows) {
		// cleanup_obs_bridge();
		// NO VMA!!!! (idk if it's better or not but i couldnt figure out how to make it work without creating a dedicated separate device and pool)
		if (mvkobjs.obs_target.img) {
			vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.obs_target.view, nullptr);
			vkDestroyImage(mvkobjs.vkdevice.device, mvkobjs.obs_target.img, nullptr);
			vkFreeMemory(mvkobjs.vkdevice.device, mvkobjs.obs_target.memory, nullptr);
		}
	}

	if (rview::core::exrtex.at(0).img) {
		vkDestroyImageView(mvkobjs.vkdevice.device, rview::core::exrtex.at(0).imgview, nullptr);
		vkDestroySampler(mvkobjs.vkdevice.device, rview::core::exrtex.at(0).imgsampler, nullptr);
		vmaDestroyImage(mvkobjs.alloc, rview::core::exrtex.at(0).img, rview::core::exrtex.at(0).alloc);
	}

	vmaDestroyBuffer(mvkobjs.alloc, rview::core::global_materials.buffer, rview::core::global_materials.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, rview::core::g_rawIndexSSBO.buffer, rview::core::g_rawIndexSSBO.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, rview::core::g_primitiveRegistrySSBO.buffer, rview::core::g_primitiveRegistrySSBO.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, rview::core::g_modelRegistrySSBO.buffer, rview::core::g_modelRegistrySSBO.alloc);
	vmaDestroyBuffer(mvkobjs.alloc, g_morphDeltaSSBO.buffer, g_morphDeltaSSBO.alloc);

	for (auto&& x : rview::core::globalCameraUBO)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : rview::core::g_indirectCommandBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	vmaDestroyBuffer(mvkobjs.alloc, rview::core::g_indirectCountBuffers.buffer, rview::core::g_indirectCountBuffers.alloc);

	for (auto&& x : rview::core::globalInstanceBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : rview::core::globalIndirectBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : rview::core::globalEntityStateBuffers)
		vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);

	for (auto&& x : model_manager::g_gpu_joint_buffers) {
		if (x.buffer)
			vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);
	}

	for (auto&& x : g_morphWeightBuffers) {
		if (x.buffer)
			vmaDestroyBuffer(mvkobjs.alloc, x.buffer, x.alloc);
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

	for (auto& dead : g_asset_death_row) {
		if (dead.vbo.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(mvkobjs.alloc, dead.vbo.buffer, dead.vbo.alloc);
		}

		for (auto& tex : dead.textures) {
			vmaDestroyImage(mvkobjs.alloc, tex.img, tex.alloc);
			vkDestroyImageView(mvkobjs.vkdevice.device, tex.imgview, nullptr);
		}
	}

	g_asset_death_row.clear();

	for (uint32_t i = 1; i < model_manager::AnimPoolRegistry::MAX_POOLS; ++i) {
		if (model_manager::g_animPoolRegistry.is_active[i]) {
			auto& pool = model_manager::g_animPoolRegistry.pools[i];

			if (pool.bakedAnimVBO.buffer != VK_NULL_HANDLE) {
				vmaDestroyBuffer(mvkobjs.alloc, pool.bakedAnimVBO.buffer, pool.bakedAnimVBO.alloc);
				pool.bakedAnimVBO.buffer = VK_NULL_HANDLE;
			}
		}
	}

	model_manager::g_cpuModels.clear();
	gexitq.fdie();
	std::ranges::for_each(mvkobjs.sbelts, [&mvkobjs](auto& x) { x.free(mvkobjs.alloc); });

	StagingBelt* workerBelt = nullptr;
	while (g_worker_belts.pop(workerBelt)) {
		workerBelt->free(mvkobjs.alloc);
		delete workerBelt;
	}

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
	if (g_gpuQueryPool != VK_NULL_HANDLE) {
		vkDestroyQueryPool(mvkobjs.vkdevice.device, g_gpuQueryPool, nullptr);
		g_gpuQueryPool = VK_NULL_HANDLE;
		g_gpuTimestampPeriod = 0.0f;
		for (bool& u : g_gpuQueryUsed)
			u = false;
	}
	vkb::destroy_device(mvkobjs.vkdevice);
	vkb::destroy_surface(mvkobjs.inst.instance, mvkobjs.surface);
	vkb::destroy_instance(mvkobjs.inst);
}

void vkrenderer::setsize(rvkbucket& mvkobjs, unsigned int w, unsigned int h) {
	mvkobjs.width = w;
	mvkobjs.height = h;

	if (!w || !h)
		persviewproj.at(1) = glm::perspective(glm::radians(static_cast<float>(mvkobjs.schain.extent.width)),
											  static_cast<float>(mvkobjs.schain.extent.height) / static_cast<float>(mvkobjs.height),
											  1.0f,
											  2.0f);
	else
		persviewproj.at(1) =
			glm::perspective(rview::core::fov, static_cast<float>(mvkobjs.width) / static_cast<float>(mvkobjs.height), 0.002f, 2000.0f);
}
bool vkrenderer::create_obs_target(rvkbucket& mvkobjs) {
	if constexpr (rview::core::platform::is_windows) {
		VkPhysicalDeviceIDProperties idProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
		VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		props2.pNext = &idProps;
		vkGetPhysicalDeviceProperties2(mvkobjs.vkdevice.physical_device, &props2);

		if (!idProps.deviceLUIDValid) {
			std::cerr << "Vulkan side: Device lacks valid LUID." << std::endl;
			return false;
		}

		mvkobjs.obs_target.shared_handle =
			obs_bridge::create_d3d_shared_texture(mvkobjs.obs_target.width, mvkobjs.obs_target.height, idProps.deviceLUID);

		if (!mvkobjs.obs_target.shared_handle)
			return false;

		VkExternalMemoryHandleTypeFlagBitsKHR handleType =
			obs_bridge::use_d3d12 ? VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHR : VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;

		VkExternalMemoryImageCreateInfo extMemInfo{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
		extMemInfo.handleTypes = handleType;

		VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		imageInfo.pNext = &extMemInfo;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
		imageInfo.extent = {mvkobjs.obs_target.width, mvkobjs.obs_target.height, 1};
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
	}

	return true;
}
void vkrenderer::copy_engine_to_obs(
	VkCommandBuffer cmdBuffer, rvkbucket& mvkobjs, VkImage engine_source_img, VkImage obs_target_img, uint32_t width, uint32_t height) {

	if constexpr (rview::core::platform::is_windows) {
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
		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

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
					   engine_source_img,
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   obs_target_img,
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1,
					   &blit,
					   VK_FILTER_LINEAR);

		srcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.dstAccessMask = 0;
		dstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		dstBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

		VkImageMemoryBarrier postBarriers[] = {srcBarrier, dstBarrier};
		vkCmdPipelineBarrier(
			cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 2, postBarriers);
	}
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
						if (is_down)
							mvkobjs.mshutdown = true;

						break;
				}

				break;
			}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (e.button.button == SDL_BUTTON_RIGHT)
				input_state[Key_RightClick] = true;

			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (e.button.button == SDL_BUTTON_RIGHT)
				input_state[Key_RightClick] = false;

			break;

		case SDL_EVENT_WINDOW_MINIMIZED:
			[[unlikely]] {
				SDL_Event min_event;
				while (SDL_WaitEvent(&min_event)) {
					if (min_event.type == SDL_EVENT_WINDOW_RESTORED) {
						if constexpr (rview::core::platform::is_android) {
							if (!mvkobjs.surface) {
								SDL_Vulkan_CreateSurface(mvkobjs.wind, mvkobjs.inst, nullptr, &mvkobjs.surface);
							}
							recreateswapchain(mvkobjs);
						}
						break;
					}

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

				if (fname_view.ends_with(".glb") || fname_view.ends_with(".vrm") || fname_view.ends_with(".GLB") || fname_view.ends_with(".VRM")) {

					uint32_t currentDropID =
						g_dropCounter.fetch_add(1, std::memory_order_relaxed) % 8192; // TODO constexpr number of cancellation tokens [[MAGIC]]
					gctokens[currentDropID].store(false, std::memory_order_release);

					float mx, my;
					SDL_GetMouseState(&mx, &my);
					glm::vec4 viewport(0.0f, 0.0f, (float)mvkobjs.width, (float)mvkobjs.height);
					glm::vec3 nearPt = glm::unProject(glm::vec3(mx, viewport.w - my, 0.0f), persviewproj[0], persviewproj[1], viewport);
					glm::vec3 farPt = glm::unProject(glm::vec3(mx, viewport.w - my, 1.0f), persviewproj[0], persviewproj[1], viewport);
					glm::vec3 rayDir = glm::normalize(farPt - nearPt);
					float t = (glm::abs(rayDir.y) > 0.001f) ? (0.0f - rview::core::camwpos.y) / rayDir.y : -1.0f;
					glm::vec3 hitPos = (t > 0.0f && t <= 50.0f) ? (rview::core::camwpos + rayDir * t) : (rview::core::camwpos + rayDir * 50.0f);

					vkrenderer::DropSession session{};
					session.dropID = currentDropID;
					session.spawnPos = hitPos;
					session.filename = std::string(fname_view);
					vkrenderer::g_activeDrops.emplace_back(std::move(session));
					vkrenderer::g_openDropModal = true;

					if (gaiojobs.load(std::memory_order_acquire) == 0) {
						gjstrings.reset();
					}

					const char* fname = gjstrings.pstring(fname_view);
					gaiojobs.fetch_add(1, std::memory_order_release);
					SDL_Log("[load %u] enqueue %s", currentDropID, fname);
					gjobs.enqueue(
						[fname, currentDropID, &mvkobjs]() noexcept {
							SDL_Log("[load %u] job started", currentDropID);
							sjguard ioguard(gaiojobs);

							try {
								model_manager::g_progress_queue.push({currentDropID, model_manager::ParseStep::parsing});

								model_manager::StagingModelData staging = model_manager::parse_model_to_staging(fname, currentDropID);
								SDL_Log("[load %u] parsed: %zu meshes, %zu textures", currentDropID, staging.meshes.size(), staging.textures.size());
								if (gctokens[currentDropID].load(std::memory_order_acquire)) {
									model_manager::g_progress_queue.push({currentDropID, model_manager::ParseStep::cancelled});
									return;
								}

								model_manager::g_progress_queue.push({currentDropID, model_manager::ParseStep::done});

								staging.dropID = currentDropID;
								staging.skipui = false;

								if (!staging.meshes.empty()) {
									std::vector<uint32_t> uploadedTexIDs(staging.textures.size(), 0xFFFFFFFF);

									staging.assignedModelID = model_manager::commit_staging_to_vulkan(mvkobjs, staging, uploadedTexIDs);
									SDL_Log("[load %u] committed as model %u", currentDropID, staging.assignedModelID);
									auto it = model_manager::g_cpuModels.find(staging.assignedModelID);
									staging.defaultPool = (it != model_manager::g_cpuModels.end() && !it->second.defaultClips.empty())
															  ? it->second.defaultClips[0]
															  : model_manager::FALLBACK_POOL_HANDLE;

									while (!vkrenderer::pending_staging_models.push(std::move(staging))) {
										SDL_Log("[load %u] handed to main thread", currentDropID);
										if (!gjobs.isrun())
											break;
										rview::core::cpu_relax();
									}
								}
							} catch (const std::exception& e) {
								std::cerr << "Failed to load " << fname << ": " << e.what() << "\n";
								model_manager::g_progress_queue.push({currentDropID, model_manager::ParseStep::cancelled});
							}
						},
						jprio::low);
				}

				break;
			}
		case SDL_EVENT_DROP_COMPLETE:
			break;
		case SDL_EVENT_FINGER_DOWN:
		case SDL_EVENT_FINGER_UP:
			if constexpr (rview::core::platform::is_android) {
				rview::core::g_is_swiping = false; // Reset swipe state on touch start/end
			}
			break;

		case SDL_EVENT_FINGER_MOTION:
			if constexpr (rview::core::platform::is_android) {
				// Use a small deadzone so tapping the screen doesn't accidentally trigger a swipe
				if (std::abs(e.tfinger.dx) > 0.002f || std::abs(e.tfinger.dy) > 0.002f) {
					rview::core::g_is_swiping = true;
				}

				if (rview::core::g_is_swiping) {
					// Sensitivity: 180.0f means a full-screen swipe rotates the camera 180 degrees.
					// Adjust these values to make the swipe feel natural.
					float touch_sens_x = 180.0f;
					float touch_sens_y = 180.0f;

					// Match your mouse rotation math
					rview::core::azimuth += e.tfinger.dx * touch_sens_x;
					rview::core::elevation -= e.tfinger.dy * touch_sens_y;

					// Clamp elevation to prevent rolling/flipping over the top
					rview::core::elevation = std::clamp(rview::core::elevation, -89.0f, 89.0f);
				}
			}
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
	gm.lspawn(drop.loadID);
	if (mvkobjs.transferQidx != mvkobjs.graphicsQidx) {
		std::vector<VkBufferMemoryBarrier2> acquires(drop.buffersToAcquire.size());
		for (size_t i = 0; i < drop.buffersToAcquire.size(); ++i) {
			acquires[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			acquires[i].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
			acquires[i].srcAccessMask = 0;
			acquires[i].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			acquires[i].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
			acquires[i].srcQueueFamilyIndex = mvkobjs.transferQidx;
			acquires[i].dstQueueFamilyIndex = mvkobjs.graphicsQidx;
			acquires[i].buffer = drop.buffersToAcquire[i];
			acquires[i].offset = 0;
			acquires[i].size = VK_WHOLE_SIZE;
		}

		std::vector<VkImageMemoryBarrier2> imgAcquires(drop.imagesToAcquire.size());
		for (size_t i = 0; i < drop.imagesToAcquire.size(); ++i) {
			imgAcquires[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			imgAcquires[i].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
			imgAcquires[i].srcAccessMask = 0;
			imgAcquires[i].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			imgAcquires[i].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
			imgAcquires[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imgAcquires[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imgAcquires[i].srcQueueFamilyIndex = mvkobjs.transferQidx;
			imgAcquires[i].dstQueueFamilyIndex = mvkobjs.graphicsQidx;
			imgAcquires[i].image = drop.imagesToAcquire[i];
			imgAcquires[i].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		}

		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.bufferMemoryBarrierCount = static_cast<uint32_t>(acquires.size());
		dep.pBufferMemoryBarriers = acquires.data();
		dep.imageMemoryBarrierCount = static_cast<uint32_t>(imgAcquires.size());
		dep.pImageMemoryBarriers = imgAcquires.data();

		vkCmdPipelineBarrier2(c, &dep);
	}
	uint32_t modelID = drop.assignedModelID;
	uint32_t defaultPool = drop.defaultPool;

	auto it = model_manager::g_cpuModels.find(modelID);
	if (it != model_manager::g_cpuModels.end()) {
		it->second.refCount += drop.requested_instances;
	}

	for (uint32_t i = 0; i < drop.requested_instances; ++i) {
		model_manager::Entity new_ent = model_manager::g_registry.create_entity(modelID);
		if (!model_manager::g_registry.is_valid(new_ent))
			continue;

		uint32_t dense_idx = model_manager::g_registry.get_dense_index(new_ent);
		g_scene.poolHandles[dense_idx] = defaultPool;

		if (drop.demo_modifier) {
			drop.demo_modifier(i, drop.requested_instances, new_ent, drop.spawn_position);
		} else {
			model_manager::g_registry.position(new_ent) = drop.spawn_position;
		}
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
	uint32_t dropID = g_activeDrops[id].dropID;
	gctokens[dropID].store(true, std::memory_order_release);
	g_activeDrops.erase(g_activeDrops.begin() + id);
}

void vkrenderer::cancelall() {
	for (auto& drop : g_activeDrops) {
		gctokens[drop.dropID].store(true, std::memory_order_release);
	}

	g_activeDrops.clear();
}

void vkrenderer::movecam(rvkbucket& mvkobjs) {
	static ImGuiIO* io = &ImGui::GetIO();
	float mx, my;
	const SDL_MouseButtonFlags mouseMask = SDL_GetMouseState(&mx, &my);
	const bool isRightClick = (mouseMask & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT));
	const bool isLeftClick = (mouseMask & SDL_BUTTON_MASK(SDL_BUTTON_LEFT));
	float speed = 2.0f;
	rview::core::camfor = (float(input_state[Key_W]) * speed) - (float(input_state[Key_S]) * speed);
	rview::core::camright = (float(input_state[Key_D]) * speed) - (float(input_state[Key_A]) * speed);
	rview::core::camup = (float(input_state[Key_E]) * speed) - (float(input_state[Key_Q]) * speed);

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
		rview::core::azimuth += rx * 0.1f;
		rview::core::elevation -= ry * 0.1f;
		rview::core::elevation = std::clamp(rview::core::elevation, -89.0f, 89.0f);
	} else {
		vkrenderer::mousex = static_cast<int>(mx);
		vkrenderer::mousey = static_cast<int>(my);
	}

	persviewproj.at(0) = vkcam::getview();

	bool can_raycast = (!io->WantCaptureMouse && isLeftClick && !isRightClick);

	if constexpr (rview::core::platform::is_android) {
		can_raycast = can_raycast && !rview::core::g_is_swiping;
	}

	if (can_raycast) {
		glm::vec4 viewport(0.0f, 0.0f, (float)mvkobjs.width, (float)mvkobjs.height);

		glm::vec3 nearPt = glm::unProject(glm::vec3(mx, viewport.w - my, 0.0f), persviewproj.at(0), persviewproj.at(1), viewport);
		glm::vec3 farPt = glm::unProject(glm::vec3(mx, viewport.w - my, 1.0f), persviewproj.at(0), persviewproj.at(1), viewport);

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

static constexpr uint32_t ASSET_UPLOAD_BUDGET = 8 * 1024 * 1024; // MAGIC but not that much

void vkrenderer::sync_assets_to_gpu(VkCommandBuffer cmd, rvkbucket& mvkobjs) {
	if (!g_assets.requiresUpload.load(std::memory_order_acquire))
		return;

	// never wait, try next frame
	std::unique_lock<std::mutex> lock(g_assets.registryMutex, std::try_to_lock);

	if (!lock.owns_lock())
		return;

	g_assets.requiresUpload.store(false, std::memory_order_release);

	bool descriptors_need_update = false;
	std::vector<VkBufferMemoryBarrier2> barriers;

	auto add_barrier = [&](VkBuffer buf) {
		if (buf == VK_NULL_HANDLE)
			return;

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
		uint32_t chunkBytes = std::min(newBytesToUpload, ASSET_UPLOAD_BUDGET);
		VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(chunkBytes);

		if (beltOffset == VK_WHOLE_SIZE) {
			g_assets.requiresUpload.store(true, std::memory_order_release);
		} else {
			if (currentByteSize > rview::core::g_rawIndexCapacity) {
				uint32_t newCapacity = std::max(rview::core::g_rawIndexCapacity * 2, currentByteSize + (1024 * 1024 * 16));
				GpuBuffer newBuffer;
				vkvbo::init(mvkobjs,
							newBuffer,
							newCapacity,
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

				if (rview::core::g_rawIndexSSBO.buffer != VK_NULL_HANDLE) {
					VkBufferCopy copyRegion{0, 0, rview::core::g_rawIndexOffset};
					vkCmdCopyBuffer(cmd, rview::core::g_rawIndexSSBO.buffer, newBuffer.buffer, 1, &copyRegion);

					vkrenderer::CondemnedAsset dead;
					dead.vbo = rview::core::g_rawIndexSSBO;
					vkrenderer::g_asset_death_row.push_back(std::move(dead));
				}

				rview::core::g_rawIndexSSBO = newBuffer;
				rview::core::g_rawIndexCapacity = newCapacity;
				descriptors_need_update = true;
			}

			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset,
						g_assets.globalRawIndices.data() + rview::core::g_rawIndexOffset,
						chunkBytes);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, chunkBytes);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_rawIndexOffset, chunkBytes};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, rview::core::g_rawIndexSSBO.buffer, 1, &tailRegion);

			rview::core::g_rawIndexOffset += chunkBytes;

			if (rview::core::g_rawIndexOffset < currentByteSize) {
				g_assets.requiresUpload.store(true, std::memory_order_release);
			}

			add_barrier(rview::core::g_rawIndexSSBO.buffer);
		}
	}

	uint32_t currentMorphByteSize = static_cast<uint32_t>(g_assets.globalMorphBytes.size());
	uint32_t newMorphBytesToUpload = currentMorphByteSize - g_morphDeltaOffset;

	if (newMorphBytesToUpload > 0) {
		if (currentMorphByteSize > g_morphDeltaCapacity) {
			uint32_t newCapacity = std::max(g_morphDeltaCapacity * 2, currentMorphByteSize + (1024 * 1024 * 8));

			GpuBuffer newBuffer;
			vkvbo::init(mvkobjs,
						newBuffer,
						newCapacity,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			if (g_morphDeltaSSBO.buffer != VK_NULL_HANDLE) {
				VkBufferCopy copyRegion{0, 0, g_morphDeltaOffset};
				vkCmdCopyBuffer(cmd, g_morphDeltaSSBO.buffer, newBuffer.buffer, 1, &copyRegion);
			}

			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(newMorphBytesToUpload);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset,
						g_assets.globalMorphBytes.data() + g_morphDeltaOffset,
						newMorphBytesToUpload);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, newMorphBytesToUpload);

			VkBufferCopy tailRegion{beltOffset, g_morphDeltaOffset, newMorphBytesToUpload};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, newBuffer.buffer, 1, &tailRegion);

			if (g_morphDeltaSSBO.buffer != VK_NULL_HANDLE) {
				vkrenderer::CondemnedAsset dead;
				dead.vbo = g_morphDeltaSSBO;
				vkrenderer::g_asset_death_row.push_back(std::move(dead));
			}

			g_morphDeltaSSBO = newBuffer;

			g_morphDeltaSSBO = newBuffer;
			g_morphDeltaCapacity = newCapacity;
			descriptors_need_update = true;
		} else {
			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(newMorphBytesToUpload);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset,
						g_assets.globalMorphBytes.data() + g_morphDeltaOffset,
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
			vkvbo::init(mvkobjs,
						newBuffer,
						newByteCap,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			if (rview::core::g_primitiveRegistrySSBO.buffer != VK_NULL_HANDLE) {
				VkBufferCopy copyRegion{0, 0, rview::core::g_primOffset * sizeof(PrimitiveMetadata)};
				vkCmdCopyBuffer(cmd, rview::core::g_primitiveRegistrySSBO.buffer, newBuffer.buffer, 1, &copyRegion);
			}

			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset,
						g_assets.primitives.data() + rview::core::g_primOffset,
						uploadBytes);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, uploadBytes);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_primOffset * sizeof(PrimitiveMetadata), uploadBytes};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, newBuffer.buffer, 1, &tailRegion);

			if (rview::core::g_primitiveRegistrySSBO.buffer != VK_NULL_HANDLE) {
				vkrenderer::CondemnedAsset dead;
				dead.vbo = rview::core::g_primitiveRegistrySSBO;
				vkrenderer::g_asset_death_row.push_back(std::move(dead));
			}

			rview::core::g_primitiveRegistrySSBO = newBuffer;
			rview::core::g_primitiveRegistrySSBO = newBuffer;
			rview::core::g_primCapacity = newCap;
			descriptors_need_update = true;
		} else {
			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset,
						g_assets.primitives.data() + rview::core::g_primOffset,
						uploadBytes);
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
			vkvbo::init(mvkobjs,
						newBuffer,
						newByteCap,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			if (rview::core::g_modelRegistrySSBO.buffer != VK_NULL_HANDLE) {
				VkBufferCopy copyRegion{0, 0, rview::core::g_modelOffset * sizeof(ModelMetadata)};
				vkCmdCopyBuffer(cmd, rview::core::g_modelRegistrySSBO.buffer, newBuffer.buffer, 1, &copyRegion);
			}

			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(
				mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.models.data() + rview::core::g_modelOffset, uploadBytes);
			vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, uploadBytes);

			VkBufferCopy tailRegion{beltOffset, rview::core::g_modelOffset * sizeof(ModelMetadata), uploadBytes};
			vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, newBuffer.buffer, 1, &tailRegion);

			if (rview::core::g_modelRegistrySSBO.buffer != VK_NULL_HANDLE) {
				vkrenderer::CondemnedAsset dead;
				dead.vbo = rview::core::g_modelRegistrySSBO;
				vkrenderer::g_asset_death_row.push_back(std::move(dead));
			}

			rview::core::g_modelRegistrySSBO = newBuffer;
			rview::core::g_modelRegistrySSBO = newBuffer;
			rview::core::g_modelCapacity = newCap;
			descriptors_need_update = true;
		} else {
			VkDeviceSize beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(uploadBytes);
			std::memcpy(
				mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset, g_assets.models.data() + rview::core::g_modelOffset, uploadBytes);
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
void flush_staging_and_resume(VkCommandBuffer& cmd, VkQueue queue, StagingBelt& belt) {
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

	// AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	vkQueueWaitIdle(queue);

	belt.reset();

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);
}
bool vkrenderer::draw(rvkbucket& mvkobjs) {
	ZoneScoped;
	mscope mfs;
	static size_t faketick{0};
	faketick++;
	if(faketick%2000 == 0)
		jdump();

	if constexpr (rview::core::platform::is_android) {
		if (!mvkobjs.surface || !mvkobjs.schain.swapchain) {
			SDL_Event e;
			while (SDL_PollEvent(&e)) {
				ImGui_ImplSDL3_ProcessEvent(&e);
				sdlevent(mvkobjs, e);
			}
			rview::core::cpu_relax();
			return true; // skip
		}
	}


	VkDeviceSize countOffset = rview::core::currentFrame * sizeof(uint32_t);
	gm.stage(mstage::fence);
	if (vkWaitForFences(mvkobjs.vkdevice.device, 1, &mvkobjs.fencez.at(rview::core::currentFrame), VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}

	if (g_gpuQueryPool != VK_NULL_HANDLE && g_gpuQueryUsed[rview::core::currentFrame]) {
		uint64_t ts[2] = {0, 0};
		VkResult qres = vkGetQueryPoolResults(
			mvkobjs.vkdevice.device,
			g_gpuQueryPool,
			static_cast<uint32_t>(2 * rview::core::currentFrame),
			2,
			sizeof(ts),
			ts,
			sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT);
		if (qres == VK_SUCCESS && ts[1] >= ts[0]) {
			gm.gpu(static_cast<uint64_t>(static_cast<double>(ts[1] - ts[0]) * static_cast<double>(g_gpuTimestampPeriod)));
		}
		g_gpuQueryUsed[rview::core::currentFrame] = false;
	}

	gm.stage(mstage::setup);
	// good spot as any
	mvkobjs.sbelts[rview::core::currentFrame].reset();

	if (vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.fencez.at(rview::core::currentFrame)) != VK_SUCCESS)
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

	if (vkBeginCommandBuffer(c, &cmdbgninfo) != VK_SUCCESS)
		return false;

	if (g_gpuQueryPool != VK_NULL_HANDLE) {
		vkCmdResetQueryPool(c, g_gpuQueryPool, static_cast<uint32_t>(2 * rview::core::currentFrame), 2);
		vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, g_gpuQueryPool, static_cast<uint32_t>(2 * rview::core::currentFrame));
	}


	double tick = static_cast<double>(SDL_GetTicks()) / 1000.0;
	rview::core::tickdiff = tick - vkrenderer::mlasttick;
	rview::core::frametime = mframetimer.stop();
	mframetimer.start();

	uint32_t imgidx = 0;
	gm.stage(mstage::acquire);
	VkResult res = vkAcquireNextImageKHR(
		mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX, mvkobjs.semaphorez[0][rview::core::currentFrame], VK_NULL_HANDLE, &imgidx);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_ERROR_SURFACE_LOST_KHR) {
		if constexpr (rview::core::platform::is_android) {
			if (res == VK_ERROR_SURFACE_LOST_KHR) {
				vkb::destroy_surface(mvkobjs.inst.instance, mvkobjs.surface);
				mvkobjs.surface = VK_NULL_HANDLE;

				SDL_Event push_min;
				push_min.type = SDL_EVENT_WINDOW_MINIMIZED;
				SDL_PushEvent(&push_min);
				return true;
			}
		}
		return recreateswapchain(mvkobjs);
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}


	if (mvkobjs.schain.extent.width == 0 || mvkobjs.schain.extent.height == 0)
		return true;

	gm.stage(mstage::events);
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		ImGui_ImplSDL3_ProcessEvent(&e);
		sdlevent(mvkobjs, e);
	}

	gm.stage(mstage::intake);
	// maybe this is not worth it...
	model_manager::ProgressUpdate update;

	while (model_manager::g_progress_queue.pop(update)) {
		SDL_Log("[load %u] step %d", update.requestID, static_cast<int>(update.step));
		for (auto& drop : g_activeDrops) {
			if (drop.dropID == update.requestID) {
				drop.currentStep = update.step;
				break;
			}
		}
	}

	model_manager::StagingModelData ephemeralstage;

	while (pending_staging_models.pop(ephemeralstage)) {
		SDL_Log("[load %u] main thread received", ephemeralstage.dropID);
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

	gm.stage(mstage::commit);
	//couldnt think of somewhere better to leave this
	if (!g_commit_queue.empty()) {
		commitspawn(mvkobjs, c, std::move(g_commit_queue.front()));

		auto* deadStaging = new model_manager::StagingModelData(std::move(g_commit_queue.front()));
		g_commit_queue.erase(g_commit_queue.begin());

		greaper.enqueue([deadStaging]() noexcept { delete deadStaging; });
	}

	gm.stage(mstage::deathrow);
	model_manager::OrphanedAnimBuffer orphaned;

	while (model_manager::g_orphanedAnimBuffers.pop(orphaned)) {
		vkrenderer::CondemnedAsset dead;
		dead.vbo = orphaned.vbo;
		dead.bindlessID = orphaned.bindlessID;
		vkrenderer::g_asset_death_row.push_back(std::move(dead));
	}

	for (int i = (int)g_asset_death_row.size() - 1; i >= 0; --i) {
		auto& dead = g_asset_death_row[i];

		if (dead.framesRemaining == 0) {
			if (dead.vbo.buffer != VK_NULL_HANDLE) {
				vmaDestroyBuffer(mvkobjs.alloc, dead.vbo.buffer, dead.vbo.alloc);
			}

			if (dead.bindlessID != 0xFFFFFFFF) {
				std::lock_guard<std::mutex> lock(rview::core::global_buffers.mtx);
				rview::core::global_buffers.free_slots.push(dead.bindlessID);
			}

			for (auto& tex : dead.textures) {
				vmaDestroyImage(mvkobjs.alloc, tex.img, tex.alloc);
				vkDestroyImageView(mvkobjs.vkdevice.device, tex.imgview, nullptr);
			}

			if (!dead.textures.empty()) {
				auto* deadTextures = new std::vector<texdata>(std::move(dead.textures));
				greaper.enqueue([deadTextures]() noexcept { delete deadTextures; });
			}

			g_asset_death_row[i] = std::move(g_asset_death_row.back());
			g_asset_death_row.pop_back();
		} else {
			dead.framesRemaining--;
		}
	}

	gm.stage(mstage::kills);
	const bool anim_quiet = model_manager::AnimQuiet();
	gm.quiet(anim_quiet);

	if (anim_quiet) {
		for (auto& kill : g_kill_queue) {
			model_manager::g_registry.destroy_entity(kill.entity);
			auto it = model_manager::g_cpuModels.find(kill.modelID);

			if (it != model_manager::g_cpuModels.end()) {
				it->second.refCount--;

				if (it->second.refCount == 0) {
					model_manager::g_animPoolRegistry.DestroyPools(it->second.defaultClips.data(), it->second.defaultClips.size());

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

					auto* deadCPUAsset = new model_manager::CPUModelAsset(std::move(it->second));
					model_manager::g_cpuModels.erase(it);

					greaper.enqueue([deadCPUAsset]() noexcept { delete deadCPUAsset; });
				}
			}
		}

		g_kill_queue.clear();
	}

	gm.stage(mstage::statechg);
	rview::core::global_frame_counter.fetch_add(1, std::memory_order_relaxed);

	model_manager::ProcessDeferredStateChanges(rview::core::global_frame_counter.load(std::memory_order_relaxed));
	model_manager::g_animPoolRegistry.SyncNames();

	gm.stage(mstage::quiet);
	if (anim_quiet) {
		model_manager::PoolMigrationReq migReq;
		std::vector<uint32_t>* bakeBatch = nullptr;
		constexpr size_t BAKE_BATCH = 32;

		auto flushBakeBatch = [&]() {
			if (!bakeBatch)
				return;

			auto* batch = bakeBatch;
			bakeBatch = nullptr;

			gjobs.enqueue(
				[batch, &mvkobjs]() noexcept {
					ZoneScopedN("uibake");
					try {
						size_t stagingBytes = 0;

						for (uint32_t handle : *batch) {
							if (auto* p = model_manager::g_animPoolRegistry.GetPoolMutable(handle))
								stagingBytes += p->finalJointDQs.size() * sizeof(DualQuatScale);
						}

						model_manager::ChunkUploader uploader(mvkobjs, stagingBytes);

						for (uint32_t handle : *batch) {
							auto* p = model_manager::g_animPoolRegistry.GetPoolMutable(handle);
							if (!p)
								continue;

							// ===================
							//    HEAVY BAKING   |
							// ===================

							size_t byteSize = p->finalJointDQs.size() * sizeof(DualQuatScale);
							if (byteSize == 0)
								continue;

							size_t alignedSize = (byteSize + 255) & ~255;
							GpuBuffer targetVBO;
							vkvbo::init(mvkobjs, targetVBO, alignedSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
							uploader.upload(targetVBO, p->finalJointDQs.data(), byteSize);
							p->bakedAnimVBO = targetVBO;
						}

						uploader.flush();
					} catch (const std::exception& e) {
						std::cerr << "anim bake failed: " << e.what() << "\n";
					}

					for (uint32_t handle : *batch) {
						while (!model_manager::g_poolUploadQueue.push({handle})) {
							rview::core::cpu_relax();
						}
					}

					delete batch;
				},
				jprio::low);
		};

		while (model_manager::g_poolMigrationQueue.pop(migReq)) {
			auto* pool = model_manager::g_animPoolRegistry.GetPoolMutable(migReq.poolHandle);

			if (!pool || pool->migrationState != model_manager::PoolMigrationState::STABLE)
				continue;

			if (migReq.targetDomain == model_manager::ExecutionDomain::GPU_BAKED && pool->bakedAnimVBO.buffer == VK_NULL_HANDLE) {
				pool->migrationState = model_manager::PoolMigrationState::MIGRATING_TO_GPU;

				if (!bakeBatch) {
					bakeBatch = new std::vector<uint32_t>();
					bakeBatch->reserve(BAKE_BATCH);
				}

				bakeBatch->push_back(migReq.poolHandle);

				if (bakeBatch->size() == BAKE_BATCH)
					flushBakeBatch();
			} else if (migReq.targetDomain == model_manager::ExecutionDomain::CPU_BAKED && pool->bakedAnimVBO.buffer != VK_NULL_HANDLE) {
				vkrenderer::CondemnedAsset deadAsset;
				deadAsset.vbo = pool->bakedAnimVBO;
				deadAsset.bindlessID = pool->bakedBindlessID;
				vkrenderer::g_asset_death_row.push_back(std::move(deadAsset));

				pool->bakedAnimVBO.buffer = VK_NULL_HANDLE;
				pool->bakedBindlessID = 0xFFFFFFFF;
				pool->pipelineFlags = model_manager::ANIM_FLAG_CPU_BAKED;
				pool->domain = model_manager::ExecutionDomain::CPU_BAKED;
			}
		}

		flushBakeBatch();

		model_manager::PoolPurgeReq purgeReq;
		static std::vector<model_manager::PoolPurgeReq> busyPurges;
		busyPurges.clear();

		while (model_manager::g_poolPurgeQueue.pop(purgeReq)) {
			auto* pool = model_manager::g_animPoolRegistry.GetPoolMutable(purgeReq.poolHandle);

			if (pool && pool->migrationState != model_manager::PoolMigrationState::STABLE) {
				busyPurges.push_back(purgeReq);
				continue;
			}

			if (pool && pool->retainCpuData) {
				pool->retainCpuData = false;

				model_manager::PurgePoolCPUDataAsync(*pool);
			}
		}

		for (const auto& busy : busyPurges) {
			model_manager::g_poolPurgeQueue.push(busy);
		}

		model_manager::PoolUploadReq upReq;

		while (model_manager::g_poolUploadQueue.pop(upReq)) {
			auto* pool = model_manager::g_animPoolRegistry.GetPoolMutable(upReq.poolHandle);
			if (!pool || pool->migrationState != model_manager::PoolMigrationState::MIGRATING_TO_GPU)
				continue;

			if (pool->condemned || pool->bakedAnimVBO.buffer == VK_NULL_HANDLE) {
				pool->migrationState = model_manager::PoolMigrationState::STABLE;

				if (pool->condemned)
					model_manager::g_animPoolRegistry.DestroyPools(&upReq.poolHandle, 1);

				continue;
			}

			uint32_t newID;
			{
				std::lock_guard<std::mutex> lock(rview::core::global_buffers.mtx);
				if (!rview::core::global_buffers.free_slots.empty()) {
					newID = rview::core::global_buffers.free_slots.front();
					rview::core::global_buffers.free_slots.pop();
				} else {
					newID = rview::core::globalBufferCounter.fetch_add(1, std::memory_order_relaxed);
				}
			}
			pool->bakedBindlessID = newID;

			VkDescriptorBufferInfo bufInfo{pool->bakedAnimVBO.buffer, 0, pool->bakedAnimVBO.size};
			VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			write.dstSet = rview::core::globalBindlessSet;
			write.dstBinding = 19;
			write.dstArrayElement = pool->bakedBindlessID;
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write.descriptorCount = 1;
			write.pBufferInfo = &bufInfo;

			{
				std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
				vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &write, 0, nullptr);
			}

			if (!pool->finalJointDQs.empty())
				model_manager::reap(std::move(pool->finalJointDQs));

			pool->pipelineFlags = model_manager::ANIM_FLAG_GPU_BAKED;
			pool->domain = model_manager::ExecutionDomain::GPU_BAKED;
			pool->migrationState = model_manager::PoolMigrationState::STABLE;
		}
	}

	gm.stage(mstage::poolprep);
	uint32_t write_frame = rview::core::currentFrame;
	uint32_t read_frame = (rview::core::currentFrame + rview::core::MAX_FRAMES_IN_FLIGHT - 1) % rview::core::MAX_FRAMES_IN_FLIGHT;

	model_manager::MarkPoolsInUse();

	if (model_manager::g_anim_jobs_pending[read_frame].load(std::memory_order_acquire) == 0) {
		model_manager::CatchUpPools(read_frame);
		model_manager::UpdateAttachments();
		model_manager::upload_joint_matrices(mvkobjs, read_frame);
	}

	gm.stage(mstage::dispatch);
	model_manager::g_anim_time_accumulator += rview::core::tickdiff;

	if (model_manager::g_anim_jobs_pending[write_frame].load(std::memory_order_acquire) == 0) {
		model_manager::DispatchAnimationJobs(model_manager::g_anim_time_accumulator, write_frame);
		model_manager::g_anim_time_accumulator = 0.0f;
	}

	gm.stage(mstage::instances);
	update_dynamic_instances(mvkobjs, read_frame);
	gm.stage(mstage::assetsync);
	sync_assets_to_gpu(c, mvkobjs);
	gm.stage(mstage::record);
	movecam(mvkobjs);
	moveplayer();

	{
		void* data;
		vmaMapMemory(mvkobjs.alloc, rview::core::globalCameraUBO[rview::core::currentFrame].alloc, &data);

		std::byte* dst = static_cast<std::byte*>(data);
		std::memcpy(dst, persviewproj.data(), 2 * sizeof(glm::mat4));
		glm::vec4 campos4(rview::core::camwpos, 0.0f);
		std::memcpy(dst + (2 * sizeof(glm::mat4)), &campos4, sizeof(glm::vec4));

		vmaFlushAllocation(mvkobjs.alloc, rview::core::globalCameraUBO[rview::core::currentFrame].alloc, 0, VK_WHOLE_SIZE);

		vmaUnmapMemory(mvkobjs.alloc, rview::core::globalCameraUBO[rview::core::currentFrame].alloc);
	}

	if constexpr (rview::core::platform::is_windows)
		std::call_once(obswrite, [&]() { vkrenderer::write_obs(mvkobjs); });





	// let that sync in
	currentgraph.add_pass([&mvkobjs, c, imgidx] {
		if constexpr (rdebug::has_tracy) {
			TracyVkZone(rdebug::tracyCTX, c, "syncs");
		}

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

	currentgraph.add_pass([c, countOffset] {
		if constexpr (rdebug::has_tracy) {
			TracyVkZone(rdebug::tracyCTX, c, "compute/cull");
		}

		vkCmdFillBuffer(c, rview::core::g_indirectCountBuffers.buffer, countOffset, sizeof(uint32_t), 0);

		VkBufferMemoryBarrier2 fillBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		fillBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		fillBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		fillBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		fillBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		fillBarrier.buffer = rview::core::g_indirectCountBuffers.buffer;
		fillBarrier.size = sizeof(uint32_t);

		VkDependencyInfo fillDep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		fillDep.bufferMemoryBarrierCount = 1;
		fillDep.pBufferMemoryBarriers = &fillBarrier;
		vkCmdPipelineBarrier2(c, &fillDep);

		uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

		if (active_instances == 0)
			return;

		vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_COMPUTE, rview::core::globalcullpline);
		vkCmdBindDescriptorSets(
			c, VK_PIPELINE_BIND_POINT_COMPUTE, rview::core::globalPipelineLayout, 0, 1, &rview::core::globalBindlessSet, 0, nullptr);

		vkpushconstants pc{};
		pc.stride = active_instances;
		pc.frameIndex = rview::core::currentFrame;
		vkCmdPushConstants(c,
						   rview::core::globalPipelineLayout,
						   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
						   0,
						   sizeof(vkpushconstants),
						   &pc);

		vkCmdDispatch(c, (active_instances + 63) / 64, 1, 1);

		VkBufferMemoryBarrier2 computeBarriers[3] = {};
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
		computeBarriers[1].buffer = rview::core::g_indirectCountBuffers.buffer;
		computeBarriers[1].size = sizeof(uint32_t);

		computeBarriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		computeBarriers[2].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		computeBarriers[2].srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		computeBarriers[2].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		computeBarriers[2].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		computeBarriers[2].buffer = rview::core::globalInstanceBuffers[rview::core::currentFrame].buffer;
		computeBarriers[2].size = VK_WHOLE_SIZE;

		VkDependencyInfo computeDep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		computeDep.bufferMemoryBarrierCount = 3;
		computeDep.pBufferMemoryBarriers = computeBarriers;
		vkCmdPipelineBarrier2(c, &computeDep);
	});

	currentgraph.add_pass([&mvkobjs, c, imgidx, countOffset] {
		if constexpr (rdebug::has_tracy) {
			TracyVkZone(rdebug::tracyCTX, c, "rendering");
		}

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



		vkCmdSetViewport(c, 0, 1, &viewport);
		vkCmdSetScissor(c, 0, 1, &scissor);



		vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, playout::skinnedpline);
		vkCmdBindDescriptorSets(
			c, VK_PIPELINE_BIND_POINT_GRAPHICS, rview::core::globalPipelineLayout, 0, 1, &rview::core::globalBindlessSet, 0, nullptr);

		vkpushconstants pc_mdi{};
		pc_mdi.frameIndex = rview::core::currentFrame;
		vkCmdPushConstants(c,
						   rview::core::globalPipelineLayout,
						   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
						   0,
						   sizeof(vkpushconstants),
						   &pc_mdi);

		vkCmdDrawIndirectCount(c,
							   rview::core::g_indirectCommandBuffers[rview::core::currentFrame].buffer,
							   0,
							   rview::core::g_indirectCountBuffers.buffer,
							   countOffset,
							   100000,
							   sizeof(VkDrawIndirectCommand));
		
		gm.stage(mstage::ui);
		// TODO MOVE to separate async pipeline >:( i probably wont ever but eh.. the thought that counts ig
		ui::createdbgframe(mvkobjs);
		ui::createdropwidget(mvkobjs, c);
		ui::render(mvkobjs, c);
		gm.stage(mstage::graph);

		vkCmdEndRendering(c);
	});


	currentgraph.add_pass([&mvkobjs, c, imgidx] {
		if constexpr (rdebug::has_tracy) {
			TracyVkZone(rdebug::tracyCTX, c, "last sync");
		}

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

	gm.stage(mstage::graph);
	currentgraph.execute();

	if constexpr (rview::core::platform::is_windows)
		vkrenderer::copy_engine_to_obs(
			c, mvkobjs, mvkobjs.schainimgs.at(imgidx), mvkobjs.obs_target.img, mvkobjs.obs_target.width, mvkobjs.obs_target.height);

	if constexpr (rdebug::has_tracy) {
		TracyVkCollect(rdebug::tracyCTX, c);
	}

	if (g_gpuQueryPool != VK_NULL_HANDLE) {
		vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g_gpuQueryPool, static_cast<uint32_t>(2 * rview::core::currentFrame + 1));
	}

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
		gm.stage(mstage::lockwait);
		std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
		gm.stage(mstage::submit);
		auto res = vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo, mvkobjs.fencez.at(rview::core::currentFrame));

		//   vkQueueWaitIdle(mvkobjs.graphicsQ);
		if (res != VK_SUCCESS) {
			std::cout << "QUEUE ERROR : " << res << std::endl;
			return false;
		}
	}

	if (g_gpuQueryPool != VK_NULL_HANDLE) {
		g_gpuQueryUsed[rview::core::currentFrame] = true;
	}

	gm.stage(mstage::tail);
	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.semaphorez.at(1).at(imgidx);

	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &mvkobjs.schain.swapchain;

	presentinfo.pImageIndices = &imgidx;

	gm.stage(mstage::lockwait);
	auto start_lock = std::chrono::high_resolution_clock::now();

	rview::core::mtx2->lock();

	auto end_lock = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> lock_time = end_lock - start_lock;

	if (lock_time.count() > 5.0) {
		std::cout << "MAIN THREAD BLOCKED FOR: " << lock_time.count() << " ms\n";
	}

	gm.stage(mstage::present);
	res = vkQueuePresentKHR(mvkobjs.presentQ, &presentinfo);
	rview::core::mtx2->unlock();
	gm.stage(mstage::tail);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain(mvkobjs);
	} else {
		if (res != VK_SUCCESS) {
			return false;
		}
	}

	{
		const uint64_t now = mnow();
		if (g_lastVmemSample == 0 || mms(now - g_lastVmemSample) >= 1000.0) {
			g_lastVmemSample = now;
			const VkPhysicalDeviceMemoryProperties* memprops = nullptr;
			vmaGetMemoryProperties(mvkobjs.alloc, &memprops);
			if (memprops) {
				VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
				vmaGetHeapBudgets(mvkobjs.alloc, budgets);
				uint64_t sumused = 0;
				uint64_t sumbudget = 0;
				const uint32_t heapCount = memprops->memoryHeapCount < VK_MAX_MEMORY_HEAPS ? memprops->memoryHeapCount : VK_MAX_MEMORY_HEAPS;
				for (uint32_t i = 0; i < heapCount; ++i) {
					sumused += budgets[i].usage;
					sumbudget += budgets[i].budget;
				}
				gm.vmem(sumused, sumbudget);
			}
		}
	}

	if constexpr (rview::core::platform::is_windows)
		obs_bridge::send_spout();

	vkrenderer::mlasttick = tick;
	rview::core::currentFrame = (rview::core::currentFrame + 1) % rview::core::MAX_FRAMES_IN_FLIGHT;

	return true;
}
