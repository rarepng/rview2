#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION

// #define _DEBUG

#include "vkrenderer.hpp"
#include "exp/particle.hpp"

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

float map2(glm::vec3 x) {
	return std::max(x.y, 0.0f);
}
vkrenderer::vkrenderer(SDL_Window *wind, const SDL_DisplayMode *mode, bool *mshutdown, SDL_Event *e) {
	mvkobjs.wind = wind;
	mvkobjs.rdmode = mode;
	mvkobjs.e = e;
	mvkobjs.mshutdown = mshutdown;
}
bool vkrenderer::init() {
	std::srand(static_cast<int>(time(NULL)));
	mvkobjs.height = mvkobjs.schain.extent.height;
	mvkobjs.width = mvkobjs.schain.extent.width;
	if (!mvkobjs.wind)
		return false;
	if (!std::apply([](auto &&...f) {
	return (... && f());
	}, std::tuple{
		[this]() {
			return deviceinit() && initvma() && getqueue() &&
			createswapchain() && createdepthbuffer() &&
			createcommandpool() && createcommandbuffer() &&
			createrenderpass() && createframebuffer() &&
			createsyncobjects() && createpools();
		},
	}))
	return false;

	if (initui()) {
		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	} else
		return false;

	mframetimer.start();

	return true;
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
		if (!i->setup(mvkobjs, playerfname[idx], playercount[idx], playershaders[idx][0], playershaders[idx][1]))
			return false;
		for (size_t j{0}; j < playercount[idx]; j++)
			selectiondata.instancesettings.at(idx).at(j) = &i->getinst(j)->getinstancesettings();
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
rvk &vkrenderer::getvkobjs() {
	return mvkobjs;
}
bool vkrenderer::quicksetup() {
	inmenu = false;

	return true;
}
bool vkrenderer::deviceinit() {
	vkb::InstanceBuilder instbuild{};

	auto instret = instbuild.use_default_debug_messenger().request_validation_layers().require_api_version(1, 3).build();

	if (!instret) {
		std::cout << instret.full_error().type << std::endl;
		std::cout << instret.full_error().vk_result << std::endl;
		return false;
	}

	mvkobjs.inst = instret.value();

	bool res{false};
	res = SDL_Vulkan_CreateSurface(mvkobjs.wind, mvkobjs.inst, nullptr, &msurface);
	if (!res) {
		std::cout << "SDL failed to create surface" << std::endl;
		return false;
	}

	vkb::PhysicalDeviceSelector physicaldevsel{mvkobjs.inst};
	auto firstphysicaldevselret = physicaldevsel.set_surface(msurface).set_minimum_version(1, 3).select();

	VkPhysicalDeviceVulkan14Features physfeatures14;
	VkPhysicalDeviceVulkan13Features physfeatures13;
	VkPhysicalDeviceVulkan12Features physfeatures12;
	VkPhysicalDeviceVulkan11Features physfeatures11;
	VkPhysicalDeviceFeatures2 physfeatures;

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

	vkGetPhysicalDeviceFeatures2(firstphysicaldevselret.value(), &physfeatures);
	auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 3)
	                               .set_surface(msurface)
	                               .set_required_features(physfeatures.features)
	                               .set_required_features_11(physfeatures11)
	                               .set_required_features_12(physfeatures12)
	                               .set_required_features_13(physfeatures13)
	                               .set_required_features_14(physfeatures14)
	                               .select();

	mvkobjs.physdev = secondphysicaldevselret.value();

	mminuniformbufferoffsetalignment = mvkobjs.physdev.properties.limits.minUniformBufferOffsetAlignment;

	vkb::DeviceBuilder devbuilder{mvkobjs.physdev};
	auto devbuilderret = devbuilder.build();
	mvkobjs.vkdevice = devbuilderret.value();

	// VkSurfaceCapabilitiesKHR surcap;
	// vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.rdvkbdevice.physical_device, mvkobjs.rdvkbphysdev.surface,
	// &surcap); unsigned int excount{}; vkEnumerateDeviceExtensionProperties(mvkobjs.rdvkbdevice.physical_device,
	// nullptr, &excount, nullptr); std::vector<VkExtensionProperties> exvec(excount);
	// vkEnumerateDeviceExtensionProperties(mvkobjs.rdvkbdevice.physical_device, nullptr, &excount, exvec.data());
	// for(const auto& x:exvec)std::cout << "ext name: " << x.extensionName << " " << std::endl << "version: " <<
	// x.specVersion << std::endl;

	return true;
}
bool vkrenderer::initvma() {
	VmaAllocatorCreateInfo allocinfo{};
	allocinfo.physicalDevice = mvkobjs.physdev.physical_device;
	allocinfo.device = mvkobjs.vkdevice.device;
	allocinfo.instance = mvkobjs.inst.instance;
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

	return true;
}
bool vkrenderer::createpools() {
	std::array<VkDescriptorPoolSize, 3> poolz{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 24},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
		}};
	return rpool::create(poolz, mvkobjs.vkdevice.device, &mvkobjs.dpools[rvk::idxinitpool]);
}
bool vkrenderer::createdepthbuffer() {
	VkExtent3D depthimageextent = {mvkobjs.schain.extent.width, mvkobjs.schain.extent.height, 1};

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
	depthallocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vmaCreateImage(mvkobjs.alloc, &depthimginfo, &depthallocinfo, &mvkobjs.rddepthimage, &mvkobjs.rddepthimagealloc,
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

	if (vkCreateImageView(mvkobjs.vkdevice, &depthimgviewinfo, nullptr, &mvkobjs.rddepthimageview) != VK_SUCCESS) {
		return false;
	}

	return true;
}
bool vkrenderer::createswapchain() {
	vkb::SwapchainBuilder swapchainbuild{mvkobjs.vkdevice};

	VkSurfaceCapabilitiesKHR surcap;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.vkdevice.physical_device, msurface, &surcap);

	swapchainbuild.set_composite_alpha_flags((VkCompositeAlphaFlagBitsKHR)(surcap.supportedCompositeAlpha & 8 ? 8 : 1));
	swapchainbuild.set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
	auto swapchainbuilret =
	    swapchainbuild.set_old_swapchain(mvkobjs.schain).set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR).build();
	if (!swapchainbuilret) {
		return false;
	}
	vkb::destroy_swapchain(mvkobjs.schain);
	mvkobjs.schain = swapchainbuilret.value();
	return true;
}
bool vkrenderer::recreateswapchain() {
	SDL_GetWindowSize(mvkobjs.wind, &mvkobjs.width, &mvkobjs.height);

	vkDeviceWaitIdle(mvkobjs.vkdevice.device);
	framebuffer::destroy(mvkobjs.vkdevice.device, mvkobjs.fbuffers);
	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);

	mvkobjs.schain.destroy_image_views(mvkobjs.schainimgviews);

	if (!createswapchain()) {
		return false;
	}
	if (!createdepthbuffer()) {
		return false;
	}
	if (!createframebuffer()) {
		return false;
	}

	return true;
}
bool vkrenderer::createrenderpass() {
	if (!renderpass::init(mvkobjs))
		return false;
	return true;
}
bool vkrenderer::createframebuffer() {
	if (!framebuffer::create(mvkobjs))
		return false;
	return true;
}
bool vkrenderer::createcommandpool() {
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_graphics, vkb::QueueType::graphics))
		return false;
	if (!commandpool::createsametype(mvkobjs, mvkobjs.cpools_compute, vkb::QueueType::compute))
		return false;
	return true;
}
bool vkrenderer::createcommandbuffer() {
	if (!commandbuffer::create(mvkobjs, mvkobjs.cpools_graphics.at(0), mvkobjs.cbuffers_graphics))
		return false;
	if (!commandbuffer::create(mvkobjs, mvkobjs.cpools_compute.at(0), mvkobjs.cbuffers_compute))
		return false;
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
	commandbuffer::destroy(mvkobjs, mvkobjs.cpools_graphics.at(0), mvkobjs.cbuffers_graphics);
	commandbuffer::destroy(mvkobjs, mvkobjs.cpools_compute.at(0), mvkobjs.cbuffers_compute);
	commandpool::destroy(mvkobjs, mvkobjs.cpools_graphics);
	commandpool::destroy(mvkobjs, mvkobjs.cpools_compute);
	for (auto &x : mvkobjs.dpools)
		rpool::destroy(mvkobjs.vkdevice.device, x);
	framebuffer::destroy(mvkobjs.vkdevice.device, mvkobjs.fbuffers);

	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, rvk::ubolayout, nullptr);
	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, *rvk::texlayout, nullptr);
	vkDestroyDescriptorSetLayout(mvkobjs.vkdevice.device, rvk::ssbolayout, nullptr);

	for (const auto &i : mplayer)
		i->cleanuplines(mvkobjs);

	renderpass::cleanup(mvkobjs);
	for (const auto &i : mplayer)
		i->cleanupbuffers(mvkobjs);

	for (const auto &i : mplayer)
		i->cleanupmodels(mvkobjs);

	vkDestroyImageView(mvkobjs.vkdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.alloc, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);
	vmaDestroyAllocator(mvkobjs.alloc);

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
		                         static_cast<float>(mvkobjs.schain.extent.height) / static_cast<float>(mvkobjs.height), 0.01f, 60000.0f);
	else
		persviewproj.at(1) = glm::perspective(
		                         mvkobjs.fov, static_cast<float>(mvkobjs.width) / static_cast<float>(mvkobjs.height), 1.0f, 6000.0f);
}

bool vkrenderer::uploadfordraw() {
	// mvkobjs.uploadmtx->lock();

	if (vkWaitForFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence) != VK_SUCCESS)
		return false;

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                                     mvkobjs.presentsemaphore, VK_NULL_HANDLE, &imgidx);

	if (vkResetCommandBuffer(mvkobjs.cbuffers_graphics.at(0), 0) != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.cbuffers_graphics.at(0), &cmdbgninfo) != VK_SUCCESS)
		return false;

	manimupdatetimer.start();

	for (const auto &i : mplayer)
		i->uploadvboebo(mvkobjs, mvkobjs.cbuffers_graphics.at(0));

	mvkobjs.uploadubossbotime = manimupdatetimer.stop();
	if (vkEndCommandBuffer(mvkobjs.cbuffers_graphics.at(0)) != VK_SUCCESS)
		return false;

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;

	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &mvkobjs.presentsemaphore;

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.cbuffers_graphics.at(0);

	VkSemaphoreWaitInfo swinfo{};
	swinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	swinfo.pSemaphores = &mvkobjs.presentsemaphore;
	swinfo.semaphoreCount = 1;

	mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo, mvkobjs.renderfence) != VK_SUCCESS) {
		return false;
	}
	mvkobjs.mtx2->unlock();

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rendersemaphore;

	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &mvkobjs.schain.swapchain;

	presentinfo.pImageIndices = &imgidx;

	mvkobjs.mtx2->lock();
	VkResult res2 = vkQueuePresentKHR(mvkobjs.presentQ, &presentinfo);

	mvkobjs.mtx2->unlock();

	if (res2 == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain();
	} else {
		if (res2 != VK_SUCCESS) {
			return false;
		}
	}

	// mvkobjs.uploadmtx->unlock();
	return true;
}

bool vkrenderer::uploadfordraw(std::shared_ptr<playoutgeneric> &x) {
	// mvkobjs.uploadmtx->lock();
	
	if (vkWaitForFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence) != VK_SUCCESS)
		return false;

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                                     mvkobjs.presentsemaphore, VK_NULL_HANDLE, &imgidx);

	if (vkResetCommandBuffer(mvkobjs.cbuffers_graphics.at(0), 0) != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.cbuffers_graphics.at(0), &cmdbgninfo) != VK_SUCCESS)
		return false;

	manimupdatetimer.start();

	x->uploadvboebo(mvkobjs, mvkobjs.cbuffers_graphics.at(0));

	mvkobjs.uploadubossbotime = manimupdatetimer.stop();
	if (vkEndCommandBuffer(mvkobjs.cbuffers_graphics.at(0)) != VK_SUCCESS)
		return false;

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;

	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &mvkobjs.presentsemaphore;

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.cbuffers_graphics.at(0);

	mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo, mvkobjs.renderfence) != VK_SUCCESS) {
		return false;
	}
	mvkobjs.mtx2->unlock();

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rendersemaphore;

	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &mvkobjs.schain.swapchain;

	presentinfo.pImageIndices = &imgidx;

	mvkobjs.mtx2->lock();
	VkResult res2 = vkQueuePresentKHR(mvkobjs.presentQ, &presentinfo);

	mvkobjs.mtx2->unlock();

	// mvkobjs.uploadmtx->unlock();
	return true;
}

void vkrenderer::sdlevent(SDL_Event *e) {
	if (e->type == SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED) {
		while (e->type != SDL_EventType::SDL_EVENT_WINDOW_RESTORED) {
			// std::cout << e->type << std::endl;
			SDL_PollEvent(e);
		}
	}
	switch (e->type) {
	case SDL_EVENT_KEY_UP:
		switch (e->key.key) {
		case SDLK_F4:
			mvkobjs.fullscreen = !mvkobjs.fullscreen;
			SDL_SetWindowFullscreen(mvkobjs.wind, mvkobjs.fullscreen);
			break;
		case SDLK_ESCAPE:
			*mvkobjs.mshutdown = true;
			break;

		default:
			break;
		}
		break;
	case SDL_EVENT_DROP_BEGIN:
		std::cout << "BEG" << std::endl;
		SDL_RaiseWindow(mvkobjs.wind);
		if (e->drop.x)
			std::cout << "x : " << e->drop.x << std::endl;
		if (e->drop.y)
			std::cout << "y : " << e->drop.y << std::endl;
		if (e->drop.data) {
			std::cout << e->drop.data << std::endl;
			// SDL_free(&e->drop.data);
		}
		if (e->drop.source)
			std::cout << e->drop.source << std::endl;
		break;
	case SDL_EVENT_DROP_POSITION:
		std::cout << "POS" << std::endl;
		if (e->drop.x)
			std::cout << "x : " << e->drop.x << std::endl;
		if (e->drop.y)
			std::cout << "y : " << e->drop.y << std::endl;
		if (e->drop.data) {
			std::cout << e->drop.data << std::endl;
			// SDL_free(&e->drop.data);
		}
		if (e->drop.source)
			std::cout << e->drop.source << std::endl;
		break;
	case SDL_EVENT_DROP_COMPLETE:
		std::cout << "COMPLETE" << std::endl;
		if (e->drop.x)
			std::cout << "x : " << e->drop.x << std::endl;
		if (e->drop.y)
			std::cout << "y : " << e->drop.y << std::endl;
		if (e->drop.data) {
			std::cout << e->drop.data << std::endl;
		}
		if (e->drop.source)
			std::cout << e->drop.source << std::endl;
		break;
	case SDL_EVENT_DROP_FILE:
		std::cout << "FILE" << std::endl;
		if (e->drop.x)
			std::cout << "x : " << e->drop.x << std::endl;
		if (e->drop.y)
			std::cout << "y : " << e->drop.y << std::endl;
		if (e->drop.data) {
			std::cout << e->drop.data << std::endl;
			const std::string fnamebuffer = e->drop.data;
			auto f = std::async(std::launch::async, [&] {
				std::shared_ptr<playoutgeneric> newp = std::make_shared<playoutgeneric>();
				if (!newp->setup(mvkobjs, e->drop.data, 1, playershaders[0][0], playershaders[0][1]))
					std::cout << "failed to setup model" << std::endl;
				// uploadfordraw(newp);
				mplayerbuffer.push_back(newp);
			});
			// no free
		}
		if (e->drop.source)
			std::cout << e->drop.source << std::endl;
		break;
	default:
		break;
	}
}
void vkrenderer::moveplayer() {
	// currently selected
	modelsettings &s = mplayer[selectiondata.midx]->getinst(selectiondata.iidx)->getinstancesettings();
	s.msworldpos = playermoveto;
}
void vkrenderer::handleclick() {
	ImGuiIO &io = ImGui::GetIO();
	if (SDL_GetMouseState(nullptr, nullptr) < ImGuiMouseButton_COUNT) {
		io.AddMouseButtonEvent(SDL_GetMouseState(nullptr, nullptr), SDL_GetMouseState(nullptr, nullptr));
	}
	if (io.WantCaptureMouse)
		return;

	if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))) {
		mlock = !mlock;
		if (mlock) {
			SDL_SetWindowRelativeMouseMode(mvkobjs.wind, true);
		} else {
			SDL_SetWindowRelativeMouseMode(mvkobjs.wind, false);
		}
	}
}
void vkrenderer::handlemouse(double x, double y) {
	ImGuiIO &io = ImGui::GetIO();
	io.AddMousePosEvent((float)x, (float)y);
	if (io.WantCaptureMouse) {
		return;
	}

	int relativex = static_cast<int>(x) - mousex;
	int relativey = static_cast<int>(y) - mousey;

	// if (glfwGetMouseButton(mvkobjs.rdwind, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {

	//}

	if (true) { // resumed
		if (mlock) {
			persviewproj.at(0) = mcam.getview(mvkobjs);
			// persviewproj.at(1) = glm::perspective(glm::radians(static_cast<float>(mvkobjs.rdfov)),
			// static_cast<float>(mvkobjs.rdwidth) / static_cast<float>(mvkobjs.rdheight), 0.01f, 6000.0f);

			mvkobjs.azimuth += relativex / 10.0;

			mvkobjs.elevation -= relativey / 10.0;

			if (mvkobjs.elevation > 89.0) {
				mvkobjs.elevation = 89.0;
			}

			if (mvkobjs.elevation < -89.0) {
				mvkobjs.elevation = -89.0;
			}

			if (mvkobjs.azimuth > 90.0) {
				mvkobjs.azimuth = 90.0;
			}
			if (mvkobjs.azimuth < -90.0) {
				mvkobjs.azimuth = -90.0;
			}

			mousex = static_cast<int>(x);
			mousey = static_cast<int>(y);
		}
	}

	if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))) { // paused
		std::cout << x << " x " << std::endl;
	}
}
void vkrenderer::movecam() {
	ImGuiIO &io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}

	persviewproj.at(0) = mcam.getview(mvkobjs);

	mvkobjs.camfor = 0;

	if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W]) {
		mvkobjs.camfor += 200;
	}
	if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S]) {
		mvkobjs.camfor -= 200;
	}
	mvkobjs.camright = 0;
	if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A]) {
		mvkobjs.camright += 200;
	}
	if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D]) {
		mvkobjs.camright -= 200;
	}
	mvkobjs.camup = 0;
	if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_E]) {
		mvkobjs.camup += 200;
	}
	if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_Q]) {
		mvkobjs.camup -= 200;
	}
	if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
		if (true) { // resumed
			float x, y;
			SDL_GetMouseState(&x, &y);

			glm::vec4 viewport(0.0f, 0.0f, (float)mvkobjs.width, (float)mvkobjs.height);

			glm::vec3 near = glm::unProject(glm::vec3(x, -y, 0.0f), persviewproj.at(0), persviewproj.at(1), viewport);
			glm::vec3 far = glm::unProject(glm::vec3(x, -y, 1.0f), persviewproj.at(0), persviewproj.at(1), viewport);

			glm::vec3 d = glm::normalize(far - near);

			// intersection
			if (glm::abs(d.y) > 0.01f) {
				float t = (navmesh(0.0f, 0.0f) - mvkobjs.camwpos.y) / d.y;
				if (t >= 0.0f) {
					glm::vec3 h = mvkobjs.camwpos + t * d;

					playermoveto = h;

					modelsettings &s = mplayer[selectiondata.midx]->getinst(selectiondata.iidx)->getinstancesettings();
					playerlookto = glm::normalize(h - s.msworldpos);
					movediff = glm::vec2(glm::abs(h.x - s.msworldpos.x), glm::abs(h.z - s.msworldpos.z));
					if (movediff.x > 2.1f || movediff.y > 2.1f) {
						s.msworldrot.y = glm::degrees(glm::atan(playerlookto.x, playerlookto.z));
					}
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

	particle::drawcomp(mvkobjs);

	double tick = static_cast<double>(SDL_GetTicks()) / 1000.0;
	mvkobjs.tickdiff = tick - mlasttick;
	mvkobjs.frametime = mframetimer.stop();
	mframetimer.start();

	muigentimer.start();

	mvkobjs.rduigeneratetime = muigentimer.stop();

	// idfk
	for (auto it = mplayerbuffer.begin(); it != mplayerbuffer.end();) {
		uploadfordraw(*it);
		mplayer.push_back(std::move(*it));
		mplayerbuffer.erase(it);
		selectiondata.instancesettings.emplace_back();
		selectiondata.instancesettings.back().emplace_back(&mplayer.back()->getinst(0)->getinstancesettings());
	}

	// joint anims
	if (dummytick / 2) {
		for (const auto &i : mplayer)
			i->updateanims();
		dummytick = 0;
	}

	mmatupdatetimer.start();

	// joint mats
	for (const auto &i : mplayer)
		i->updatemats();

	mvkobjs.updatemattime = mmatupdatetimer.stop();

	// joint check
	if (dummytick % 2) {
		for (const auto &i : mplayer)
			i->getinst(0)->checkforupdates();
	}

	dummytick++;

	moveplayer();

	if (vkWaitForFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence) != VK_SUCCESS)
		return false;

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                                     mvkobjs.presentsemaphore, VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreateswapchain();
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}

	std::array<VkClearValue, 2> colorclearvalue{
		VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}}, // no idea why i have to put VkClearValue here but not in the
		// secon one, doesnt work without it
		{.depthStencil = {1.0f, 0}}};

	// VkRenderingInfo rinfo{};
	// rinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	// rinfo.renderArea.extent = mvkobjs.schain.extent;
	// rinfo.renderArea.offset = {0, 0};

	VkRenderPassBeginInfo rpinfo{};
	rpinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpinfo.renderPass = mvkobjs.rdrenderpass;

	rpinfo.renderArea.offset.x = 0;
	rpinfo.renderArea.offset.y = 0;
	rpinfo.renderArea.extent = mvkobjs.schain.extent;
	rpinfo.framebuffer = mvkobjs.fbuffers[imgidx];

	rpinfo.clearValueCount = 2;
	rpinfo.pClearValues = colorclearvalue.data();

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

	if (vkResetCommandBuffer(mvkobjs.cbuffers_graphics.at(0), 0) != VK_SUCCESS)
		return false;

	sdlevent(mvkobjs.e);

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.cbuffers_graphics.at(0), &cmdbgninfo) != VK_SUCCESS)
		return false;

	vkCmdBeginRenderPass(mvkobjs.cbuffers_graphics.at(0), &rpinfo, VK_SUBPASS_CONTENTS_INLINE);
	// try switching to rendering instead of renderpass
	// vkCmdBeginRendering()

	vkCmdSetViewport(mvkobjs.cbuffers_graphics.at(0), 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.cbuffers_graphics.at(0), 0, 1, &scissor);

	VkDeviceSize coffsets{0};
	vkCmdBindPipeline(mvkobjs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, particle::gpline);
	vkCmdBindVertexBuffers(mvkobjs.cbuffers_graphics.at(0), 0, 1, &particle::ssbobuffsnallocs.at(0).first, &coffsets);
	vkCmdDraw(mvkobjs.cbuffers_graphics.at(0), 8192, 1, 0, 0);

	for (const auto &i : mplayer)
		i->draw(mvkobjs);

	lifetime = static_cast<double>(SDL_GetTicks()) / 1000.0;
	lifetime2 = static_cast<double>(SDL_GetTicks()) / 1000.0;

	mui.createdbgframe(mvkobjs, selectiondata);

	mui.render(mvkobjs, mvkobjs.cbuffers_graphics.at(0));

	vkCmdEndRenderPass(mvkobjs.cbuffers_graphics.at(0));

	// animmtx.lock();
	// updatemtx.lock();

	muploadubossbotimer.start();

	for (const auto &i : mplayer)
		i->uploadubossbo(mvkobjs, persviewproj);

	mvkobjs.uploadubossbotime = muploadubossbotimer.stop();

	if (vkEndCommandBuffer(mvkobjs.cbuffers_graphics.at(0)) != VK_SUCCESS)
		return false;

	movecam();

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;
	std::array<VkSemaphore, 2> waitsemas{mvkobjs.presentsemaphore, particle::computeFinishedSemaphores.at(0)};
	submitinfo.waitSemaphoreCount = 2;
	submitinfo.pWaitSemaphores = waitsemas.data();

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.cbuffers_graphics.at(0);

	mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo, mvkobjs.renderfence) != VK_SUCCESS) {
		return false;
	}
	mvkobjs.mtx2->unlock();

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rendersemaphore;

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

	return true;
}

bool vkrenderer::drawloading() {
	if (vkWaitForFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence) != VK_SUCCESS)
		return false;

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                                     mvkobjs.presentsemaphore, VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreateswapchain();
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}

	VkClearValue colorclearvalue;
	colorclearvalue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

	VkClearValue depthvalue;
	depthvalue.depthStencil.depth = 1.0f;

	VkClearValue clearvals[] = {colorclearvalue, depthvalue};

	VkRenderPassBeginInfo rpinfo{};
	rpinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpinfo.renderPass = mvkobjs.rdrenderpass;

	rpinfo.renderArea.offset.x = 0;
	rpinfo.renderArea.offset.y = 0;
	rpinfo.renderArea.extent = mvkobjs.schain.extent;
	rpinfo.framebuffer = mvkobjs.fbuffers[imgidx];

	rpinfo.clearValueCount = 2;
	rpinfo.pClearValues = clearvals;

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

	if (vkResetCommandBuffer(mvkobjs.cbuffers_compute.at(0), 0) != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.cbuffers_compute.at(0), &cmdbgninfo) != VK_SUCCESS)
		return false;

	vkCmdBeginRenderPass(mvkobjs.cbuffers_compute.at(0), &rpinfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(mvkobjs.cbuffers_compute.at(0), 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.cbuffers_compute.at(0), 0, 1, &scissor);

	rdscene = mui.createloadingscreen(mvkobjs);
	mui.render(mvkobjs, mvkobjs.cbuffers_compute.at(0));

	vkCmdEndRenderPass(mvkobjs.cbuffers_compute.at(0));

	if (vkEndCommandBuffer(mvkobjs.cbuffers_compute.at(0)) != VK_SUCCESS)
		return false;

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;

	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &mvkobjs.presentsemaphore;

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.cbuffers_compute.at(0);

	mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo, mvkobjs.renderfence) != VK_SUCCESS) {
		return false;
	}
	mvkobjs.mtx2->unlock();

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rendersemaphore;

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
	return rdscene;
}

bool vkrenderer::drawblank() {
	if (vkWaitForFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.vkdevice.device, 1, &mvkobjs.renderfence) != VK_SUCCESS)
		return false;

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.vkdevice.device, mvkobjs.schain.swapchain, UINT64_MAX,
	                                     mvkobjs.presentsemaphore, VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreateswapchain();
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}

	VkClearValue colorclearvalue;
	colorclearvalue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

	VkClearValue depthvalue;
	depthvalue.depthStencil.depth = 1.0f;

	VkClearValue clearvals[] = {colorclearvalue, depthvalue};

	VkRenderPassBeginInfo rpinfo{};
	rpinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpinfo.renderPass = mvkobjs.rdrenderpass;

	rpinfo.renderArea.offset.x = 0;
	rpinfo.renderArea.offset.y = 0;
	rpinfo.renderArea.extent = mvkobjs.schain.extent;
	rpinfo.framebuffer = mvkobjs.fbuffers[imgidx];

	rpinfo.clearValueCount = 2;
	rpinfo.pClearValues = clearvals;

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

	if (vkResetCommandBuffer(mvkobjs.cbuffers_graphics.at(0), 0) != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.cbuffers_graphics.at(0), &cmdbgninfo) != VK_SUCCESS)
		return false;

	vkCmdBeginRenderPass(mvkobjs.cbuffers_graphics.at(0), &rpinfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(mvkobjs.cbuffers_graphics.at(0), 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.cbuffers_graphics.at(0), 0, 1, &scissor);

	vkCmdEndRenderPass(mvkobjs.cbuffers_graphics.at(0));

	if (vkEndCommandBuffer(mvkobjs.cbuffers_graphics.at(0)) != VK_SUCCESS)
		return false;

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;

	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &mvkobjs.presentsemaphore;

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.cbuffers_graphics.at(0);

	mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.graphicsQ, 1, &submitinfo, mvkobjs.renderfence) != VK_SUCCESS) {
		return false;
	}
	mvkobjs.mtx2->unlock();

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rendersemaphore;

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
	return true;
}
