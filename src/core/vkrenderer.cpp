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

#include "renderpass.hpp"
#include "vk/commandbuffer.hpp"
#include "vk/commandpool.hpp"
#include "vk/framebuffer.hpp"
#include "vksyncobjects.hpp"
// #ifdef _DEBUG
// #include "logger.hpp"
// #endif
#include <SDL3/SDL_vulkan.h>
// #include <unistd.h>

#include "exp/particle.hpp"

// might revert the whole module thing
// maybe was not the best idea at this time
import rview.rvk.tex;

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {

	std::cerr << "\n[VALIDATION ERROR]: " << pCallbackData->pMessage << "\n"
	          << std::endl;

	return VK_FALSE;
}

void vkrenderer::immediate_submit(
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

	mvkobjs.mtx2->lock();
	vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(mvkobjs.graphicsQ);
	mvkobjs.mtx2->unlock();

	vkFreeCommandBuffers(mvkobjs.vkdevice.device, mvkobjs.cpools_graphics.at(0),
	                     1, &cbuffer);
}

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

	auto immediate_submit = [&](std::function<void(VkCommandBuffer)> &&fn) {
		VkCommandBufferAllocateInfo allocInfo{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = cmdPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cbuffer;
		vkAllocateCommandBuffers(objs.vkdevice.device, &allocInfo, &cbuffer);

		VkCommandBufferBeginInfo beginInfo{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cbuffer, &beginInfo);

		fn(cbuffer);

		vkEndCommandBuffer(cbuffer);

		VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cbuffer;

		// wait idle needed
		//  objs.mtx2->lock();
		vkQueueSubmit(objs.graphicsQ, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(objs.graphicsQ);
		// objs.mtx2->unlock();

		vkFreeCommandBuffers(objs.vkdevice.device, cmdPool, 1, &cbuffer);
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

		immediate_submit([&](VkCommandBuffer cmd) {
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

	return true;
}
float map2(glm::vec3 x) {
	return std::max(x.y, 0.0f);
}
vkrenderer::vkrenderer(SDL_Window *wind, const SDL_DisplayMode *mode,
                       bool *mshutdown, SDL_Event *e) {
	mvkobjs.wind = wind;
	mvkobjs.rdmode = mode;
	mvkobjs.e = e;
	mvkobjs.mshutdown = mshutdown;
}
bool vkrenderer::init() {
	std::srand(static_cast<int>(time(NULL)));
	if (!mvkobjs.wind)
		return false;
	if (!std::apply(
	[](auto &&...f) {
	return (... && f());
	}, std::tuple{[this]() {
		return deviceinit() && initvma() && getqueue() &&
		       createswapchain() && createdepthbuffer() &&
		       createcommandpool() && createcommandbuffer() &&
		       createsyncobjects() && createpools() && initcpuQs() &&
		       init_global_samplers(mvkobjs) &&
		       init_dummy_textures(mvkobjs,
		                           mvkobjs.cpools_graphics.at(0)) &&
		       rview::rvk::tex::load_env_map(mvkobjs, mvkobjs.exrtex.at(0),
		                                     mvkobjs.exrdset,
		                                     mvkobjs.hdrlayout);
		// might switch to passing mvk idk, maybe replace the whole mvkobjs
		// bucket with something more functional
	}}))
	return false;
	mvkobjs.height = mvkobjs.schain.extent.height;
	mvkobjs.width = mvkobjs.schain.extent.width;

	if (initui()) {
		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	} else
		return false;

	mframetimer.start();

	return true;
}
bool vkrenderer::initcpuQs() {
	return mvkobjs.sbelt.init(mvkobjs.alloc, 256 * 1024 * 1024);
}
bool vkrenderer::initscene() {
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
ui *vkrenderer::getuihandle() {
	return &mui;
}

bool vkrenderer::deviceinit() {
	vkb::InstanceBuilder instbuild{};

	auto instret = instbuild.use_default_debug_messenger()
	               .request_validation_layers()
	               .set_debug_callback(debugCallback)
	               .require_api_version(1, 4)
	               .build();
	// auto instret = instbuild.require_api_version(1, 4).build();

	if (!instret) {
		std::cout << instret.full_error().type << std::endl;
		std::cout << instret.full_error().vk_result << std::endl;
		return false;
	}
	mvkobjs.inst = instret.value();

	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(instret.value(), &gpuCount, nullptr);
	std::vector<VkPhysicalDevice> gpus(gpuCount);
	vkEnumeratePhysicalDevices(instret.value(), &gpuCount, gpus.data());

	for (auto gpu : gpus) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(gpu, &props);
		std::cout << "available GPU: " << props.deviceName << std::endl;
	}

	bool res{false};
	res =
	    SDL_Vulkan_CreateSurface(mvkobjs.wind, mvkobjs.inst, nullptr, &msurface);
	if (!res) {
		std::cout << "SDL failed to create surface" << std::endl;
		return false;
	}

	vkb::PhysicalDeviceSelector physicaldevsel{mvkobjs.inst};
	auto firstphysicaldevselret =
	    physicaldevsel.set_surface(msurface).set_minimum_version(1, 4).select();
	if (!firstphysicaldevselret) {
		std::cout << "CRITICAL ERROR: First GPU Selection Failed!" << std::endl;
		std::cout << "Reason: " << firstphysicaldevselret.error().message()
		          << std::endl;
		return false;
	}
	VkPhysicalDeviceVulkan14Features physfeatures14{};
	VkPhysicalDeviceVulkan13Features physfeatures13{};
	VkPhysicalDeviceVulkan12Features physfeatures12{};
	VkPhysicalDeviceVulkan11Features physfeatures11{};
	VkPhysicalDeviceFeatures2 physfeatures{};

	physfeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	physfeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	physfeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	physfeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	physfeatures14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

	physfeatures.pNext = &physfeatures11;
	physfeatures11.pNext = &physfeatures12;
	physfeatures12.pNext = &physfeatures13;
	physfeatures13.pNext = &physfeatures14;
	physfeatures14.pNext = VK_NULL_HANDLE;
	vkb::PhysicalDeviceSelector second_selector{mvkobjs.inst};

	VkPhysicalDeviceRayQueryFeaturesKHR ray_query_feats = {};
	ray_query_feats.sType =
	    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	ray_query_feats.rayQuery = VK_TRUE;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR as_feats = {};
	as_feats.sType =
	    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	as_feats.accelerationStructure = VK_TRUE;

	physfeatures12.bufferDeviceAddress = VK_TRUE;
	physfeatures12.runtimeDescriptorArray = VK_TRUE;
	physfeatures12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	physfeatures12.shaderFloat16 = VK_TRUE;

	vkGetPhysicalDeviceFeatures2(firstphysicaldevselret.value(), &physfeatures);
	auto secondphysicaldevselret =
	    second_selector.set_minimum_version(1, 4)
	    .set_surface(msurface)
	    .set_required_features(physfeatures.features)
	    .set_required_features_11(physfeatures11)
	    .set_required_features_12(physfeatures12)
	    .set_required_features_13(physfeatures13)
	    .set_required_features_14(physfeatures14)
	    .add_required_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME)
	    .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
	    .add_required_extension(
	        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
	    .add_required_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
	    .add_required_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME)
	    .add_required_extension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)
	    .add_required_extension_features(ray_query_feats)
	    .add_required_extension_features(as_feats)
	    .select();
	// //get last discrete device [[[for optimus laptops]]] [[[bad fix]]]
	// if(gpus.size() > 1){
	// 	for(const auto& gpu:gpus){
	// 		VkPhysicalDeviceProperties props;
	// 		vkGetPhysicalDeviceProperties(gpu, &props);
	// 		if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	// 			mvkobjs.physdev = vkb::PhysicalDevice{instret,gpu};
	// 	}
	// }else{
	if (!secondphysicaldevselret) {
		std::cerr << "couldnt find appropriate gpu "
		          << secondphysicaldevselret.error().message() << std::endl;
		return false;
	}

	mvkobjs.physdev = secondphysicaldevselret.value();

	vkb::DeviceBuilder devbuilder{mvkobjs.physdev};
	auto devbuilderret = devbuilder.build();
	if (!devbuilderret) {
		std::cerr << "couldnt create device " << devbuilderret.error().message()
		          << "\n";
		return false;
	}
	mvkobjs.vkdevice = devbuilderret.value();

	// }

	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(mvkobjs.physdev, &props);
	std::cout << "Using GPU: " << props.deviceName << std::endl;

	mminuniformbufferoffsetalignment =
	    mvkobjs.physdev.properties.limits.minUniformBufferOffsetAlignment;

	// VkSurfaceCapabilitiesKHR surcap;
	// vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.rdvkbdevice.physical_device,
	// mvkobjs.rdvkbphysdev.surface, &surcap); unsigned int excount{};
	// vkEnumerateDeviceExtensionProperties(mvkobjs.rdvkbdevice.physical_device,
	// nullptr, &excount, nullptr); std::vector<VkExtensionProperties>
	// exvec(excount);
	// vkEnumerateDeviceExtensionProperties(mvkobjs.rdvkbdevice.physical_device,
	// nullptr, &excount, exvec.data()); for(const auto& x:exvec)std::cout << "ext
	// name: " << x.extensionName << " " << std::endl << "version: " <<
	// x.specVersion << std::endl;

	return true;
}
bool vkrenderer::initvma() {
	VmaAllocatorCreateInfo allocinfo{};
	allocinfo.vulkanApiVersion = VK_API_VERSION_1_4;
	allocinfo.physicalDevice = mvkobjs.physdev.physical_device;
	allocinfo.device = mvkobjs.vkdevice.device;
	allocinfo.instance = mvkobjs.inst.instance;
	//dbg only
	allocinfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	if (vmaCreateAllocator(&allocinfo, &mvkobjs.alloc) != VK_SUCCESS) {
		return false;
	}
	return true;
}
bool vkrenderer::getqueue() {
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
bool vkrenderer::createpools() {
	std::array<VkDescriptorPoolSize, 3> poolz{
		{	{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 24},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
		}};
	return rpool::create(poolz, mvkobjs.vkdevice.device,
	                     &mvkobjs.dpools[rvkbucket::idxinitpool]);
}
bool vkrenderer::createdepthbuffer() {
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
bool vkrenderer::createswapchain() {
	vkb::SwapchainBuilder swapchainbuild{mvkobjs.vkdevice};

	VkSurfaceCapabilitiesKHR surcap;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.vkdevice.physical_device,
	        msurface, &surcap);

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
bool vkrenderer::recreateswapchain() {
	SDL_GetWindowSize(mvkobjs.wind, &mvkobjs.width, &mvkobjs.height);

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);
	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview,
	                   nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage,
	                mvkobjs.rddepthimagealloc);

	mvkobjs.schain.destroy_image_views(mvkobjs.schainimgviews);

	if (!createswapchain()) {
		return false;
	}
	if (!createdepthbuffer()) {
		return false;
	}
	return true;
}
bool vkrenderer::createcommandpool() {
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
bool vkrenderer::createcommandbuffer() {
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
bool vkrenderer::createsyncobjects() {
	if (!vksyncobjects::init(mvkobjs))
		return false;
	return true;
}
bool vkrenderer::initui() {
	if (!mui.init(mvkobjs))
		return false;
	return true;
}
void vkrenderer::cleanup() {

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);

	particle::destroyeveryting(mvkobjs);

	mui.cleanup(mvkobjs);

	vksyncobjects::cleanup(mvkobjs);
	for (auto &x : mvkobjs.dpools)
		rpool::destroy(mvkobjs.vkdevice.device, x);

	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, rvkbucket::ubolayout,
	                             nullptr);
	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, *rvkbucket::texlayout,
	                             nullptr);
	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, rvkbucket::ssbolayout,
	                             nullptr);
	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, rvkbucket::hdrlayout,
	                             nullptr);

	for (const auto &i : mplayer)
		i->cleanuplines(mvkobjs);

	renderpass::cleanup(mvkobjs);
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

	destroyDummy(mvkobjs.defaults.purple);
	destroyDummy(mvkobjs.defaults.white);
	destroyDummy(mvkobjs.defaults.normal);
	destroyDummy(mvkobjs.defaults.black);

	for (const auto &i : mvkobjs.samplerz)
		vkDestroySampler(mvkobjs.vkdevice, i, nullptr);

	mvkobjs.schain.destroy_image_views(mvkobjs.schainimgviews);
	vkb::destroy_swapchain(mvkobjs.schain);

	vkb::destroy_device(mvkobjs.vkdevice);
	vkb::destroy_surface(mvkobjs.inst.instance, msurface);
	vkb::destroy_instance(mvkobjs.inst);
}

void vkrenderer::setsize(unsigned int w, unsigned int h) {
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

bool vkrenderer::uploadfordraw(VkCommandBuffer cbuffer) {
	manimupdatetimer.start();

	for (const auto &i : mplayer)
		i->uploadvboebo(mvkobjs, cbuffer);
	return true;
}

bool vkrenderer::uploadfordraw(std::shared_ptr<playoutgeneric> &x) {
	manimupdatetimer.start();

	x->uploadvboebo(mvkobjs,
	                mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame));

	mvkobjs.uploadubossbotime = manimupdatetimer.stop();

	return true;
}

void vkrenderer::sdlevent(const SDL_Event& e) {
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
				if (is_down) *mvkobjs.mshutdown = true;
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
					*mvkobjs.mshutdown = true;
					break;
				}
			}
			break;
		}

	case SDL_EVENT_DROP_FILE:
		[[unlikely]]
		{
			std::string fname = e.drop.data;
			pending_loads.push_back(std::async(std::launch::async, [this, fname]() {
				auto newp = std::make_shared<playoutgeneric>();

				bool success = newp->setup(mvkobjs, fname.c_str(), 1,
				                           playershaders[0][0], playershaders[0][1]);

				if (success) {
					std::lock_guard<std::mutex> lock(load_mutex);
					mplayerbuffer.push_back(newp);
				} else {
					std::cout << "model not added, probably wrong format. \nonly binary gltf files (.glb) are accepted. Provided was: " << fname << std::endl;
				}
			}));
			break;
		}
	case SDL_EVENT_DROP_COMPLETE:
		break;
	}
}
void vkrenderer::moveplayer() {
	// currently selected
	modelsettings &s = mplayer[selectiondata.midx]
	                   ->getinst(selectiondata.iidx)
	                   ->getinstancesettings();
	s.msworldpos = playermoveto;
}

void vkrenderer::movecam() {
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
				playermoveto = h;

				modelsettings &s = mplayer[selectiondata.midx]
				                   ->getinst(selectiondata.iidx)
				                   ->getinstancesettings();
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

void vkrenderer::checkforanimupdates() {
	while (!*mvkobjs.mshutdown) {
		muidrawtimer.start();
		// updatemtx.lock();
		for (const auto &i : mplayer)
			i->getinst(0)->checkforupdates();
		mvkobjs.rduidrawtime = muidrawtimer.stop();
	}
}

void vkrenderer::updateanims() {
	while (!*mvkobjs.mshutdown) {
		manimupdatetimer.start();
		for (const auto &i : mplayer)
			i->updateanims();
		mvkobjs.updateanimtime = manimupdatetimer.stop();
	}
}

bool vkrenderer::draw() {

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

	particle::drawcomp(mvkobjs);

	double tick = static_cast<double>(SDL_GetTicks()) / 1000.0;
	mvkobjs.tickdiff = tick - mlasttick;
	mvkobjs.frametime = mframetimer.stop();
	mframetimer.start();

	muigentimer.start();

	mvkobjs.rduigeneratetime = muigentimer.stop();

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(
	                   mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                   mvkobjs.semaphorez[0][rvkbucket::currentFrame], VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreateswapchain();
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}

	std::array<VkClearValue, 2> colorclearvalue{
		VkClearValue{
			.color = {
				0.0f, 0.0f, 0.0f,
				1.0f
			}}, // no idea why i have to put VkClearValue
		// here but not in the
		// secon one, doesnt work without it
		{.depthStencil = {1.0f, 0}}};

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(mvkobjs.schain.extent.height);
	viewport.width = static_cast<float>(mvkobjs.schain.extent.width);
	viewport.height = -static_cast<float>(mvkobjs.schain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	if (mvkobjs.schain.extent.width == 0 || mvkobjs.schain.extent.height == 0)
		return false;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = mvkobjs.schain.extent;

	if (vkResetCommandBuffer(
	            mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), 0) !=
	        VK_SUCCESS)
		return false;

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		ImGui_ImplSDL3_ProcessEvent(&e);
		sdlevent(e);
	}
	if (!pending_loads.empty()) {
		std::erase_if(pending_loads, [](auto& f) {
			return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		});
	}
	movecam();

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(
	            mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), &cmdbgninfo) !=
	        VK_SUCCESS)
		return false;

	// idfk
	for (auto it = mplayerbuffer.begin(); it != mplayerbuffer.end();) {
		uploadfordraw(*it);
		mplayer.push_back(std::move(*it));
		mplayerbuffer.erase(it);
		selectiondata.instancesettings.emplace_back();
		selectiondata.instancesettings.back().emplace_back(
		                                  &mplayer.back()->getinst(0)->getinstancesettings());
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

	// joint check
	// if (dummytick % 2) {
	for (const auto &i : mplayer)
		i->getinst(0)->checkforupdates();
	// }

	// dummytick++;

	moveplayer();

	VkBufferMemoryBarrier2 particleBarrier{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
	particleBarrier.buffer = particle::ssbobuffsnallocs.at(0).first;
	particleBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	particleBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	particleBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
	particleBarrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
	particleBarrier.size = VK_WHOLE_SIZE;
	particleBarrier.offset = 0;

	VkMemoryBarrier2 uploadBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
	uploadBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	uploadBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	uploadBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
	uploadBarrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
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
	depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

	depthBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
	                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthBarrier.srcAccessMask = 0;
	depthBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
	                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthBarrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	VkImageMemoryBarrier2 barriers[] = {imageBarrier, depthBarrier};
	VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dep.memoryBarrierCount = 1;
	dep.pMemoryBarriers = &uploadBarrier;
	dep.bufferMemoryBarrierCount = 1;
	dep.pBufferMemoryBarriers = &particleBarrier;
	dep.imageMemoryBarrierCount = 2;
	dep.pImageMemoryBarriers = barriers;
	vkCmdPipelineBarrier2(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
	                      &dep);

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

	vkCmdBeginRendering(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
	                    &render_info);

	vkCmdSetViewport(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), 0, 1,
	                 &viewport);
	vkCmdSetScissor(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), 0, 1,
	                &scissor);

	VkDeviceSize coffsets{0};
	vkCmdBindPipeline(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
	                  VK_PIPELINE_BIND_POINT_GRAPHICS, particle::gpline);
	vkCmdBindVertexBuffers(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
	                       0, 1, &particle::ssbobuffsnallocs.at(0).first,
	                       &coffsets);
	vkCmdDraw(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), 8192, 1, 0,
	          0);

	for (const auto &i : mplayer)
		i->draw(mvkobjs);
	// VkDrawIndexedIndirectCommand();

	lifetime = static_cast<double>(SDL_GetTicks()) / 1000.0;
	lifetime2 = static_cast<double>(SDL_GetTicks()) / 1000.0;

	mui.createdbgframe(mvkobjs, selectiondata);

	mui.render(mvkobjs, mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame));

	vkCmdEndRendering(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame));
	// vkCmdEndRenderPass(mvkobjs.cbuffers_graphics.at(rvkbucket::currentFrame));

	// animmtx.lock();
	// updatemtx.lock();

	muploadubossbotimer.start();

	for (const auto &i : mplayer)
		i->uploadubossbo(mvkobjs, persviewproj, mvkobjs.camwpos);

	mvkobjs.uploadubossbotime = muploadubossbotimer.stop();

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

	vkCmdPipelineBarrier2(mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
	                      &dependency_info);

	if (vkEndCommandBuffer(
	            mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame)) != VK_SUCCESS)
		return false;


	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::array<VkPipelineStageFlags, 2> waitstage = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
	};
	submitinfo.pWaitDstStageMask = waitstage.data();
	std::array<VkSemaphore, 2> waitsemas{
		mvkobjs.semaphorez.at(0).at(rvkbucket::currentFrame),
		                  mvkobjs.semaphorez.at(2).at(rvkbucket::currentFrame)};
	submitinfo.waitSemaphoreCount = 2;
	submitinfo.pWaitSemaphores = waitsemas.data();

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.semaphorez.at(1).at(imgidx);

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers =
	    &mvkobjs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame);

	mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo,
	                  mvkobjs.fencez.at(rvkbucket::currentFrame)) != VK_SUCCESS) {
		return false;
	}
	mvkobjs.mtx2->unlock();

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
		return recreateswapchain();
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
