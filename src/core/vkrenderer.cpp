#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION


//#define _DEBUG

#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <ctime>
#include <cstdlib>
#include <glm/gtx/spline.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <vk_mem_alloc.h>
#include <iostream>

#include "renderpass.hpp"
#include "framebuffer.hpp"
#include "commandpool.hpp"
#include "commandbuffer.hpp"
#include "vksyncobjects.hpp"

#include <future>


#include "vkrenderer.hpp"
#include "vksyncobjects.hpp"
//#ifdef _DEBUG
//#include "logger.hpp"
//#endif
#include <SDL3/SDL_vulkan.h>
// #include <unistd.h>

float map2(glm::vec3 x) {
	return std::max(x.y, 0.0f);
}
vkrenderer::vkrenderer(SDL_Window* wind,const SDL_DisplayMode* mode,bool* mshutdown,SDL_Event* e) {
    mvkobjs.rdwind = wind;
    mvkobjs.rdmode = mode;
    mvkobjs.e=e;
    mvkobjs.mshutdown = mshutdown;

	mpersviewmats.emplace_back(glm::mat4{ 1.0f });
	mpersviewmats.emplace_back(glm::mat4{ 1.0f });
}
bool vkrenderer::init() {

    std::srand(static_cast<int>(time(NULL)));
	mvkobjs.rdheight = mvkobjs.rdvkbswapchain.extent.height;
    mvkobjs.rdwidth = mvkobjs.rdvkbswapchain.extent.width;
    if (!mvkobjs.rdwind)return false;
    if(!std::apply([](auto&&... f) {
        return (... && f());
    },     std::tuple{
                   [this]() { return deviceinit() && initvma() && getqueue() && createswapchain() && createdepthbuffer() && createcommandpool()
                                              && createcommandbuffer() && createrenderpass() && createframebuffer() && createsyncobjects(); },
                    })) return false;

    if(initui()){
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }else return false;

	mframetimer.start();


	return true;
}
bool vkrenderer::initscene() {

    mplayer.reserve(playerfname.size());
    mplayer.resize(playerfname.size());

    unsigned int idx{0};
	// std::cout << "CWD : " << getcwd(new char[](1024),1024) << std::endl;
    for(auto& i : mplayer){
        i=std::make_shared<playoutgeneric>();
        if (!i->setup(mvkobjs, playerfname[idx], playercount[idx],playershaders[idx][0], playershaders[idx][1]))return false;
        idx++;
    }



	return true;
}
ui* vkrenderer::getuihandle(){
	return &mui;
}
vkobjs& vkrenderer::getvkobjs() {
	return mvkobjs;
}
bool vkrenderer::quicksetup(){
	inmenu = false;

	return true;
}
bool vkrenderer::deviceinit() {
	vkb::InstanceBuilder instbuild{};

    // std::lock_guard<std::shared_mutex> lg{ *mvkobjs.mtx2 };
    // auto instret = instbuild.use_default_debug_messenger().request_validation_layers().enable_extension("VK_EXT_shader_replicated_composites").require_api_version(1, 3, 0).build();
    auto instret = instbuild.use_default_debug_messenger().request_validation_layers().require_api_version(1, 4, 309).build();
	

    // instret.value().

    mvkobjs.rdvkbinstance = instret.value();

    bool res{false};
    res=SDL_Vulkan_CreateSurface(mvkobjs.rdwind,mvkobjs.rdvkbinstance, nullptr, &msurface);



    std::cout << res << std::endl;

	vkb::PhysicalDeviceSelector physicaldevsel{ mvkobjs.rdvkbinstance };
    auto firstphysicaldevselret = physicaldevsel.set_surface(msurface).set_minimum_version(1,3).select();

    // VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT x;
    // x.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    // x.swapchainMaintenance1 = VK_FALSE;
    // x.pNext = VK_NULL_HANDLE;

    //VkPhysicalDeviceMeshShaderFeaturesEXT physmeshfeatures;
	VkPhysicalDeviceVulkan13Features physfeatures13;

    //VkPhysicalDevice8BitStorageFeatures b8storagefeature;
	VkPhysicalDeviceVulkan12Features physfeatures12;
	VkPhysicalDeviceVulkan11Features physfeatures11;
	//VkPhysicalDeviceVulkan11Features physfeatures11;
	VkPhysicalDeviceFeatures2 physfeatures;
    //b8storagefeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
	//physmeshfeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	physfeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	physfeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	physfeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	physfeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	//physmeshfeatures.pNext = &physfeatures12;
	physfeatures.pNext = &physfeatures11;
	physfeatures11.pNext = &physfeatures12;
    physfeatures12.pNext = &physfeatures13;


    // VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT x;
    // x.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT;
    // x.shaderReplicatedComposites=true;
    // x.pNext=VK_NULL_HANDLE;

    // VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT replicatedCompositesFeatures = {};
    // replicatedCompositesFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT;
    // replicatedCompositesFeatures.shaderReplicatedComposites = VK_TRUE; // !!hardcoded
    // replicatedCompositesFeatures.pNext = VK_NULL_HANDLE;

    // physfeatures13.pNext = &x;
    // physfeatures13.pNext = &replicatedCompositesFeatures;
    physfeatures13.pNext = VK_NULL_HANDLE;

	//b8storagefeature.pNext = VK_NULL_HANDLE;
    vkGetPhysicalDeviceFeatures2(firstphysicaldevselret.value(), &physfeatures);
	//std::cout << "\n\n\n\n" << physfeatures12.runtimeDescriptorArray << std::endl;
	//if (physmeshfeatures.meshShader == VK_FALSE) {
	//	std::cout << "NO mesh shader support"  << std::endl;
	//}
	//if (physmeshfeatures.taskShader == VK_FALSE) {
	//	std::cout << "NO task shader support" << std::endl;
	//}
	//physmeshfeatures.meshShader = VK_TRUE;
	//physmeshfeatures.taskShader = VK_TRUE;
	//physmeshfeatures.multiviewMeshShader = VK_FALSE;
	//physmeshfeatures.primitiveFragmentShadingRateMeshShader = VK_FALSE;
	{
		

		




	}
    // vkph

            // VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT;

// replicatedCompositesFeatures.

    //auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 3).set_surface(msurface).set_required_features(physfeatures.features).add_required_extension_features(physmeshfeatures).set_required_features_12(physfeatures12).set_required_features_13(physfeatures13).add_required_extension("VK_EXT_mesh_shader").select();
    // auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 3).set_surface(msurface).set_required_features(physfeatures.features).set_required_features_11(physfeatures11).set_required_features_12(physfeatures12).set_required_features_13(physfeatures13).add_required_extension("VK_EXT_shader_replicated_composites").add_required_extension_features(replicatedCompositesFeatures).select();
    auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 4).set_surface(msurface).set_required_features(physfeatures.features).set_required_features_11(physfeatures11).set_required_features_12(physfeatures12).set_required_features_13(physfeatures13).select();
	//auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 0).set_surface(msurface).select();

	//std::cout << "\n\n\n\n\n\n\n\n\n\n mesh shader value: " << secondphysicaldevselret.value().is_extension_present("VK_EXT_mesh_shader") << "\n\n\n\n\n";
	//std::cout << "\n\n\n\n\n\n\n\n\n\n maintenance: " << secondphysicaldevselret.value().is_extension_present("VK_EXT_swapchain_maintenance1") << "\n\n\n\n\n";
	mvkobjs.rdvkbphysdev = secondphysicaldevselret.value();


	//for (auto& x : secondphysicaldevselret.value().get_available_extensions()) { std::cout << x << std::endl; }

	mminuniformbufferoffsetalignment = mvkobjs.rdvkbphysdev.properties.limits.minUniformBufferOffsetAlignment;



    vkb::DeviceBuilder devbuilder{ mvkobjs.rdvkbphysdev };
    auto devbuilderret = devbuilder.build();
	mvkobjs.rdvkbdevice = devbuilderret.value();

    VkSurfaceCapabilitiesKHR surcap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.rdvkbdevice.physical_device,mvkobjs.rdvkbphysdev.surface,&surcap);

    // std::cout << surcap.supportedCompositeAlpha;/////////////////////////

    unsigned int excount{};

    vkEnumerateDeviceExtensionProperties(mvkobjs.rdvkbdevice.physical_device,nullptr,&excount,nullptr);
    std::vector<VkExtensionProperties> exvec(excount);

    vkEnumerateDeviceExtensionProperties(mvkobjs.rdvkbdevice.physical_device,nullptr,&excount,nullptr);

    // VK_EXT_SHADER_REPLICATED_COMPOSITES_EXTENSION_NAME;

    // for(const auto& i:exvec){
    //     std::cout << std::string(i.extensionName) << std::endl;
    // }

	return true;
}
bool vkrenderer::initvma() {
	VmaAllocatorCreateInfo allocinfo{};
	allocinfo.physicalDevice = mvkobjs.rdvkbphysdev.physical_device;
	allocinfo.device = mvkobjs.rdvkbdevice.device;
	allocinfo.instance = mvkobjs.rdvkbinstance.instance;
	if (vmaCreateAllocator(&allocinfo, &mvkobjs.rdallocator) != VK_SUCCESS) {
		return false;
	}
	return true;
}
bool vkrenderer::getqueue()
{
	auto graphqueueret = mvkobjs.rdvkbdevice.get_queue(vkb::QueueType::graphics);
	mvkobjs.rdgraphicsqueue = graphqueueret.value();

	auto presentqueueret = mvkobjs.rdvkbdevice.get_queue(vkb::QueueType::present);
	mvkobjs.rdpresentqueue = presentqueueret.value();

	return true;
}
bool vkrenderer::createdepthbuffer() {
	VkExtent3D depthimageextent = {
		mvkobjs.rdvkbswapchain.extent.width,
		mvkobjs.rdvkbswapchain.extent.height,
		1
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
	depthallocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vmaCreateImage(mvkobjs.rdallocator, &depthimginfo, &depthallocinfo, &mvkobjs.rddepthimage, &mvkobjs.rddepthimagealloc, nullptr) != VK_SUCCESS) {
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

	if (vkCreateImageView(mvkobjs.rdvkbdevice, &depthimgviewinfo, nullptr, &mvkobjs.rddepthimageview) != VK_SUCCESS) {
		return false;
	}


	return true;
}
bool vkrenderer::createswapchain() {
    vkb::SwapchainBuilder swapchainbuild{ mvkobjs.rdvkbdevice };

    VkSurfaceCapabilitiesKHR surcap;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mvkobjs.rdvkbdevice.physical_device,msurface,&surcap);

    std::cout << surcap.supportedCompositeAlpha << std::endl;/////////////////////////

    swapchainbuild.set_composite_alpha_flags((VkCompositeAlphaFlagBitsKHR)(surcap.supportedCompositeAlpha & 8 ? 8:1));
    swapchainbuild.set_desired_format({VK_FORMAT_B8G8R8A8_SRGB,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR  });
    auto swapchainbuilret = swapchainbuild.set_old_swapchain(mvkobjs.rdvkbswapchain).set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR).build();
	if (!swapchainbuilret) {
		return false;
	}
	vkb::destroy_swapchain(mvkobjs.rdvkbswapchain);
	mvkobjs.rdvkbswapchain = swapchainbuilret.value();
	return true;
}
bool vkrenderer::recreateswapchain() {
    SDL_GetWindowSize(mvkobjs.rdwind, &mvkobjs.rdwidth, &mvkobjs.rdheight);

	vkDeviceWaitIdle(mvkobjs.rdvkbdevice.device);
	framebuffer::cleanup(mvkobjs);
	vkDestroyImageView(mvkobjs.rdvkbdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.rdallocator, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);

	mvkobjs.rdvkbswapchain.destroy_image_views(mvkobjs.rdswapchainimageviews);


	if (!createswapchain()) {
		return false;
	}if (!createdepthbuffer()) {
		return false;
	}if (!createframebuffer()) {
		return false;
	}

	return true;
}
bool vkrenderer::createrenderpass() {
	if (!renderpass::init(mvkobjs))return false;
	return true;
}
bool vkrenderer::createframebuffer() {
	if (!framebuffer::init(mvkobjs))return false;
	return true;
}
bool vkrenderer::createcommandpool() {
	if (!commandpool::init(mvkobjs, mvkobjs.rdcommandpool[0]))return false;
	if (!commandpool::init(mvkobjs, mvkobjs.rdcommandpool[1]))return false;
	if (!commandpool::init(mvkobjs, mvkobjs.rdcommandpool[2]))return false;
	return true;
}
bool vkrenderer::createcommandbuffer() {
	if (!commandbuffer::init(mvkobjs,mvkobjs.rdcommandpool[0], mvkobjs.rdcommandbuffer[0]))return false;
	if (!commandbuffer::init(mvkobjs,mvkobjs.rdcommandpool[1], mvkobjs.rdcommandbuffer[1]))return false;
	return true;
}
bool vkrenderer::createsyncobjects() {
	if (!vksyncobjects::init(mvkobjs))return false;
	return true;
}
bool vkrenderer::initui() {
	if(!mui.init(mvkobjs))return false;
	return true;
}
bool vkrenderer::initgameui(){
	if (!mui.init(mvkobjs)) {
		return false;
	}
	return true;
}
void vkrenderer::cleanup() {
	vkDeviceWaitIdle(mvkobjs.rdvkbdevice.device);



	commandbuffer::cleanup(mvkobjs, mvkobjs.rdcommandpool[1], mvkobjs.rdcommandbuffer[1]);
	commandpool::cleanup(mvkobjs, mvkobjs.rdcommandpool[1]);

    for(const auto& i:mplayer)
        i->cleanupmodels(mvkobjs);
	mui.cleanup(mvkobjs);


	//cleanmainmenu();
	//cleanloading();

	vksyncobjects::cleanup(mvkobjs);
	commandbuffer::cleanup(mvkobjs, mvkobjs.rdcommandpool[0], mvkobjs.rdcommandbuffer[0]);
	commandpool::cleanup(mvkobjs, mvkobjs.rdcommandpool[0]);
	commandpool::cleanup(mvkobjs, mvkobjs.rdcommandpool[2]);
	framebuffer::cleanup(mvkobjs);


    for(const auto& i:mplayer)
        i->cleanuplines(mvkobjs);


    renderpass::cleanup(mvkobjs);
    for(const auto& i:mplayer)
        i->cleanupbuffers(mvkobjs);

	vkDestroyImageView(mvkobjs.rdvkbdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.rdallocator, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);
	vmaDestroyAllocator(mvkobjs.rdallocator);

	mvkobjs.rdvkbswapchain.destroy_image_views(mvkobjs.rdswapchainimageviews);
	vkb::destroy_swapchain(mvkobjs.rdvkbswapchain);

	vkb::destroy_device(mvkobjs.rdvkbdevice);
	vkb::destroy_surface(mvkobjs.rdvkbinstance.instance, msurface);
	vkb::destroy_instance(mvkobjs.rdvkbinstance);


}

void vkrenderer::setsize(unsigned int w, unsigned int h) {
	mvkobjs.rdwidth = w;
	mvkobjs.rdheight = h;
	if (!w || !h)
		mpersviewmats.at(1) = glm::perspective(glm::radians(static_cast<float>(mvkobjs.rdvkbswapchain.extent.width)), static_cast<float>(mvkobjs.rdvkbswapchain.extent.height) / static_cast<float>(mvkobjs.rdheight), 0.01f, 60000.0f);
	else
		mpersviewmats.at(1) = glm::perspective(mvkobjs.rdfov, static_cast<float>(mvkobjs.rdwidth) / static_cast<float>(mvkobjs.rdheight), 1.0f, 6000.0f);

}

bool vkrenderer::uploadfordraw(){

    //mvkobjs.uploadmtx->lock();


    if (vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        return false;
    }
    if (vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence) != VK_SUCCESS)return false;


    uint32_t imgidx = 0;
    VkResult res = vkAcquireNextImageKHR(mvkobjs.rdvkbdevice.device, mvkobjs.rdvkbswapchain.swapchain, UINT64_MAX, mvkobjs.rdpresentsemaphore, VK_NULL_HANDLE, &imgidx);


    if (vkResetCommandBuffer(mvkobjs.rdcommandbuffer[0], 0) != VK_SUCCESS)return false;

    VkCommandBufferBeginInfo cmdbgninfo{};
    cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[0], &cmdbgninfo) != VK_SUCCESS)return false;


    manimupdatetimer.start();

    for(const auto& i:mplayer)
        i->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);

    mvkobjs.uploadubossbotime = manimupdatetimer.stop();
    if (vkEndCommandBuffer(mvkobjs.rdcommandbuffer[0]) != VK_SUCCESS)return false;


    VkSubmitInfo submitinfo{};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitinfo.pWaitDstStageMask = &waitstage;

    submitinfo.waitSemaphoreCount = 1;
    submitinfo.pWaitSemaphores = &mvkobjs.rdpresentsemaphore;

    submitinfo.signalSemaphoreCount = 1;
    submitinfo.pSignalSemaphores = &mvkobjs.rdrendersemaphore;

    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &mvkobjs.rdcommandbuffer.at(0);

    VkSemaphoreWaitInfo swinfo{};
    swinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    swinfo.pSemaphores = &mvkobjs.rdpresentsemaphore;
    swinfo.semaphoreCount = 1;


    mvkobjs.mtx2->lock();
    if (vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence) != VK_SUCCESS) {
        return false;
    }
    mvkobjs.mtx2->unlock();


    VkPresentInfoKHR presentinfo{};
    presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentinfo.waitSemaphoreCount = 1;
    presentinfo.pWaitSemaphores = &mvkobjs.rdrendersemaphore;

    presentinfo.swapchainCount = 1;
    presentinfo.pSwapchains = &mvkobjs.rdvkbswapchain.swapchain;

    presentinfo.pImageIndices = &imgidx;


    mvkobjs.mtx2->lock();
    size_t res2 = vkQueuePresentKHR(mvkobjs.rdpresentqueue, &presentinfo);

    mvkobjs.mtx2->unlock();

	if (res2 == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain();
	} else {
		if (res2 != VK_SUCCESS) {
			return false;
		}
	}


    //mvkobjs.uploadmtx->unlock();
    return true;
}

bool vkrenderer::uploadfordraw(std::shared_ptr<playoutgeneric>& x){

    //mvkobjs.uploadmtx->lock();


    if (vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        return false;
    }
    if (vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence) != VK_SUCCESS)return false;


    uint32_t imgidx = 0;
    VkResult res = vkAcquireNextImageKHR(mvkobjs.rdvkbdevice.device, mvkobjs.rdvkbswapchain.swapchain, UINT64_MAX, mvkobjs.rdpresentsemaphore, VK_NULL_HANDLE, &imgidx);


    if (vkResetCommandBuffer(mvkobjs.rdcommandbuffer[0], 0) != VK_SUCCESS)return false;

    VkCommandBufferBeginInfo cmdbgninfo{};
    cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[0], &cmdbgninfo) != VK_SUCCESS)return false;


    manimupdatetimer.start();

    x->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);

    mvkobjs.uploadubossbotime = manimupdatetimer.stop();
    if (vkEndCommandBuffer(mvkobjs.rdcommandbuffer[0]) != VK_SUCCESS)return false;


    VkSubmitInfo submitinfo{};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitinfo.pWaitDstStageMask = &waitstage;

    submitinfo.waitSemaphoreCount = 1;
    submitinfo.pWaitSemaphores = &mvkobjs.rdpresentsemaphore;

    submitinfo.signalSemaphoreCount = 1;
    submitinfo.pSignalSemaphores = &mvkobjs.rdrendersemaphore;

    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &mvkobjs.rdcommandbuffer.at(0);

    VkSemaphoreWaitInfo swinfo{};
    swinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    swinfo.pSemaphores = &mvkobjs.rdpresentsemaphore;
    swinfo.semaphoreCount = 1;


    mvkobjs.mtx2->lock();
    if (vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence) != VK_SUCCESS) {
        return false;
    }
    mvkobjs.mtx2->unlock();


    VkPresentInfoKHR presentinfo{};
    presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentinfo.waitSemaphoreCount = 1;
    presentinfo.pWaitSemaphores = &mvkobjs.rdrendersemaphore;

    presentinfo.swapchainCount = 1;
    presentinfo.pSwapchains = &mvkobjs.rdvkbswapchain.swapchain;

    presentinfo.pImageIndices = &imgidx;


    mvkobjs.mtx2->lock();
    size_t res2 = vkQueuePresentKHR(mvkobjs.rdpresentqueue, &presentinfo);

    mvkobjs.mtx2->unlock();

    //mvkobjs.uploadmtx->unlock();
    return true;
}

void vkrenderer::sdlevent(SDL_Event* e){
	if(e->type==SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED){
		while(e->type!=SDL_EventType::SDL_EVENT_WINDOW_RESTORED){
			// std::cout << e->type << std::endl;
            SDL_PollEvent(e);
		}
	}
    switch (e->type) {
    case SDL_EVENT_KEY_UP:
        switch(e->key.key){
        case SDLK_F4:
            mvkobjs.rdfullscreen = !mvkobjs.rdfullscreen;
            SDL_SetWindowFullscreen(mvkobjs.rdwind,mvkobjs.rdfullscreen);
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
        SDL_RaiseWindow(mvkobjs.rdwind);
        if(e->drop.x)
        std::cout << "x : " << e->drop.x << std::endl;
        if(e->drop.y)
        std::cout << "y : " << e->drop.y << std::endl;
        if(e->drop.data){
            std::cout << e->drop.data << std::endl;
            // SDL_free(&e->drop.data);
        }
        if(e->drop.source)
        std::cout << e->drop.source << std::endl;
        break;
    case SDL_EVENT_DROP_POSITION:
        std::cout << "POS" << std::endl;
        if(e->drop.x)
            std::cout << "x : " << e->drop.x << std::endl;
        if(e->drop.y)
            std::cout << "y : " << e->drop.y << std::endl;
        if(e->drop.data){
            std::cout << e->drop.data << std::endl;
            // SDL_free(&e->drop.data);
        }
        if(e->drop.source)
            std::cout << e->drop.source << std::endl;
        break;
    case SDL_EVENT_DROP_COMPLETE:
        std::cout << "COMPLETE" << std::endl;
        if(e->drop.x)
            std::cout << "x : " << e->drop.x << std::endl;
        if(e->drop.y)
            std::cout << "y : " << e->drop.y << std::endl;
        if(e->drop.data){
            std::cout << e->drop.data << std::endl;
        }
        if(e->drop.source)
            std::cout << e->drop.source << std::endl;
        break;
    case SDL_EVENT_DROP_FILE:
        std::cout << "FILE" << std::endl;
        if(e->drop.x)
            std::cout << "x : " << e->drop.x << std::endl;
        if(e->drop.y)
            std::cout << "y : " << e->drop.y << std::endl;
        if(e->drop.data){
            std::cout << e->drop.data << std::endl;
            auto f = std::async(std::launch::async,[&]{
                std::shared_ptr<playoutgeneric> newp = std::make_shared<playoutgeneric>();
                newp->setup(mvkobjs,e->drop.data,1,playershaders[0][0],playershaders[0][1]);
                // uploadfordraw(newp);
                mplayerbuffer.push_back(newp);
            });
            //no free
        }
        if(e->drop.source)
            std::cout << e->drop.source << std::endl;
        break;
    default:
        break;
    }
}
void vkrenderer::moveplayer(){
    //currently selected
    modelsettings& s = mplayer[0]->getinst(0)->getinstancesettings();
    s.msworldpos = playermoveto;
}
void vkrenderer::handleclick()
{
	ImGuiIO& io = ImGui::GetIO();
    if (SDL_GetMouseState(nullptr,nullptr) >= 0 && SDL_GetMouseState(nullptr,nullptr) < ImGuiMouseButton_COUNT) {
        io.AddMouseButtonEvent(SDL_GetMouseState(nullptr,nullptr), SDL_GetMouseState(nullptr,nullptr));
	}
	if (io.WantCaptureMouse)return;
	


    if ((SDL_GetMouseState(nullptr,nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))) {
		mlock = !mlock;
        if (mlock) {
            SDL_SetWindowRelativeMouseMode(mvkobjs.rdwind,true);
        } else {
            SDL_SetWindowRelativeMouseMode(mvkobjs.rdwind,false);
		}
	}


}
void vkrenderer::handlemouse(double x, double y){
	ImGuiIO& io = ImGui::GetIO();
	io.AddMousePosEvent((float)x, (float)y);
	if (io.WantCaptureMouse) {
		return;
	}

	int relativex = static_cast<int>(x) - mousex;
	int relativey = static_cast<int>(y) - mousey;

	//if (glfwGetMouseButton(mvkobjs.rdwind, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {

	//}


    if (true) { //resumed
		if (mlock) {

			mpersviewmats.at(0) = mcam.getview(mvkobjs);
			//mpersviewmats.at(1) = glm::perspective(glm::radians(static_cast<float>(mvkobjs.rdfov)), static_cast<float>(mvkobjs.rdwidth) / static_cast<float>(mvkobjs.rdheight), 0.01f, 6000.0f);

			mvkobjs.rdazimuth += relativex / 10.0;

			mvkobjs.rdelevation -= relativey / 10.0;

			if (mvkobjs.rdelevation > 89.0) {
				mvkobjs.rdelevation = 89.0;
			}

			if (mvkobjs.rdelevation < -89.0) {
				mvkobjs.rdelevation = -89.0;
			}


			if (mvkobjs.rdazimuth > 90.0) {
				mvkobjs.rdazimuth = 90.0;
			}
			if (mvkobjs.rdazimuth < -90.0) {
				mvkobjs.rdazimuth = -90.0;
			}


			mousex = static_cast<int>(x);
			mousey = static_cast<int>(y);
		}
	}

    if ( (SDL_GetMouseState(nullptr,nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))) {//paused
		std::cout << x << " x " << std::endl;
	}



	
}
void vkrenderer::movecam() {


	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}

	mpersviewmats.at(0) = mcam.getview(mvkobjs);

	mvkobjs.rdcamforward = 0;

    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_W]) {
		mvkobjs.rdcamforward += 200;
	}
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_S]) {
		mvkobjs.rdcamforward -= 200;
	}
	mvkobjs.rdcamright = 0;
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_A]) {
		mvkobjs.rdcamright += 200;
	}
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_D]) {
		mvkobjs.rdcamright -= 200;
	}
	mvkobjs.rdcamup = 0;
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_E]) {
		mvkobjs.rdcamup += 200;
	}
    if (SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_Q]) {
		mvkobjs.rdcamup -= 200;
	}

    if (SDL_GetMouseState(nullptr,nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
        if (true) { //resumed
            float x;
            float y;
            SDL_GetMouseState(&x, &y);
			lastmousexy = glm::vec<2, double>{ x,y };
			x = (2.0 * (x / (double)mvkobjs.rdwidth)) - 1.0;
			y = (2.0 * (y / (double)mvkobjs.rdheight)) - 1.0;
			float d{ 0.0f };
			glm::vec3 cfor{ mcam.mforward };
			float rotangx{ (float)(1.5 * x * -0.523599f) };
			float rotangy{ (float)(1.5 * y * (-0.523599f * mvkobjs.rdheight / mvkobjs.rdwidth)) };
			cfor = glm::rotate(cfor, rotangx, mcam.mup);
			cfor = glm::rotate(cfor, rotangy, mcam.mright);
			for (int i{ 0 }; i < 100000; i++) {
				float dt = map2(mvkobjs.rdcamwpos + cfor * d);
				d += dt;
				if (dt < 0.00001f || d>10000.0f)break;
			}

			if (d < 10000.0f) {
                //currently selected
                modelsettings& s = mplayer[0]->getinst(0)->getinstancesettings();

				playerlookto = glm::normalize(mvkobjs.rdcamwpos + cfor * d - s.msworldpos);
				movediff = glm::vec2(glm::abs(glm::vec3((mvkobjs.rdcamwpos + cfor * d) - s.msworldpos)).x, glm::abs(glm::vec3((mvkobjs.rdcamwpos + cfor * d) - s.msworldpos)).z);

				if (movediff.x>2.1f||movediff.y>2.1f) {
					playermoveto = mvkobjs.rdcamwpos + cfor * d;
					mvkobjs.raymarchpos = mvkobjs.rdcamwpos + cfor * d;

					s.msworldrot.y = glm::degrees(glm::atan(playerlookto.x, playerlookto.z));

					playermoving = true;
				}
			}
		}
	}


}



void vkrenderer::checkforanimupdates() {

    while (!*mvkobjs.mshutdown) {

        muidrawtimer.start();
        //updatemtx.lock();
        for(const auto& i:mplayer)
        i->getinst(0)->checkforupdates();
        mvkobjs.rduidrawtime = muidrawtimer.stop();
	}

}

void vkrenderer::updateanims(){


    while (!*mvkobjs.mshutdown) {


        manimupdatetimer.start();
        for(const auto& i:mplayer)
        i->updateanims();
        mvkobjs.updateanimtime = manimupdatetimer.stop();

	}
}

bool vkrenderer::draw() {

        double tick = static_cast<double>(SDL_GetTicks())/1000.0;
		mvkobjs.tickdiff = tick - mlasttick;
		mvkobjs.frametime = mframetimer.stop();
		mframetimer.start();

		muigentimer.start();


		mvkobjs.rduigeneratetime = muigentimer.stop();


        for(auto it = mplayerbuffer.begin();it!=mplayerbuffer.end();){
            uploadfordraw(*it);
            mplayer.push_back(std::move(*it));
            mplayerbuffer.erase(it);
        }


		//joint anims
		if (dummytick / 2) {
            for(const auto& i:mplayer)
            i->updateanims();
			dummytick = 0;
		} 

		mmatupdatetimer.start();


		//joint mats
        for(const auto& i:mplayer)
        i->updatemats();

		mvkobjs.updatemattime = mmatupdatetimer.stop();
		
		//joint check
        if(dummytick%2){
            for(const auto& i:mplayer)
            i->getinst(0)->checkforupdates();



        }

		dummytick++;



        moveplayer();



		if (vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
			return false;
		}
		if (vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence) != VK_SUCCESS)return false;

		uint32_t imgidx = 0;
		VkResult res = vkAcquireNextImageKHR(mvkobjs.rdvkbdevice.device, mvkobjs.rdvkbswapchain.swapchain, UINT64_MAX, mvkobjs.rdpresentsemaphore, VK_NULL_HANDLE, &imgidx);
		if (res == VK_ERROR_OUT_OF_DATE_KHR) {
			return recreateswapchain();
		} else {
			if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
				return false;
			}
		}



		VkClearValue colorclearvalue;
        colorclearvalue.color = { {0.0f,0.0f,0.0f,0.0f } };

		VkClearValue depthvalue;
		depthvalue.depthStencil.depth = 1.0f;

		VkClearValue clearvals[] = { colorclearvalue,depthvalue };


		VkRenderPassBeginInfo rpinfo{};
		rpinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpinfo.renderPass = mvkobjs.rdrenderpass;

		rpinfo.renderArea.offset.x = 0;
		rpinfo.renderArea.offset.y = 0;
		rpinfo.renderArea.extent = mvkobjs.rdvkbswapchain.extent;
		rpinfo.framebuffer = mvkobjs.rdframebuffers[imgidx];

		rpinfo.clearValueCount = 2;
		rpinfo.pClearValues = clearvals;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
		viewport.width = static_cast<float>(mvkobjs.rdvkbswapchain.extent.width);
		viewport.height = -static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0,0 };
		scissor.extent = mvkobjs.rdvkbswapchain.extent;









		if (vkResetCommandBuffer(mvkobjs.rdcommandbuffer[0], 0) != VK_SUCCESS)return false;

        sdlevent(mvkobjs.e);

		VkCommandBufferBeginInfo cmdbgninfo{};
		cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[0], &cmdbgninfo) != VK_SUCCESS)return false;













		vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer[0], &rpinfo, VK_SUBPASS_CONTENTS_INLINE);





		vkCmdSetViewport(mvkobjs.rdcommandbuffer[0], 0, 1, &viewport);
		vkCmdSetScissor(mvkobjs.rdcommandbuffer[0], 0, 1, &scissor);
        for(const auto& i:mplayer)
        i->draw(mvkobjs);





        lifetime = static_cast<double>(SDL_GetTicks())/1000.0;
        lifetime2 = static_cast<double>(SDL_GetTicks())/1000.0;

        //currently selected
        modelsettings& settings = mplayer[0]->getinst(0)->getinstancesettings();
        mui.createdbgframe(mvkobjs, settings);

		mui.render(mvkobjs, mvkobjs.rdcommandbuffer[0]);


		vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[0]);

		//animmtx.lock();
		//updatemtx.lock();

		muploadubossbotimer.start();

        for(const auto& i:mplayer)
        i->uploadubossbo(mvkobjs, mpersviewmats);



        mvkobjs.uploadubossbotime = muploadubossbotimer.stop();



		if (vkEndCommandBuffer(mvkobjs.rdcommandbuffer[0]) != VK_SUCCESS)return false;



		movecam();


		VkSubmitInfo submitinfo{};
		submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitinfo.pWaitDstStageMask = &waitstage;

		submitinfo.waitSemaphoreCount = 1;
		submitinfo.pWaitSemaphores = &mvkobjs.rdpresentsemaphore;

		submitinfo.signalSemaphoreCount = 1;
		submitinfo.pSignalSemaphores = &mvkobjs.rdrendersemaphore;

		submitinfo.commandBufferCount = 1;
		submitinfo.pCommandBuffers = &mvkobjs.rdcommandbuffer.at(0);


		mvkobjs.mtx2->lock();
		if (vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence) != VK_SUCCESS) {
			return false;
		}
		mvkobjs.mtx2->unlock();



		VkPresentInfoKHR presentinfo{};
		presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentinfo.waitSemaphoreCount = 1;
		presentinfo.pWaitSemaphores = &mvkobjs.rdrendersemaphore;

		presentinfo.swapchainCount = 1;
		presentinfo.pSwapchains = &mvkobjs.rdvkbswapchain.swapchain;

		presentinfo.pImageIndices = &imgidx;


		mvkobjs.mtx2->lock();
		res = vkQueuePresentKHR(mvkobjs.rdpresentqueue, &presentinfo);

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

	if (vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence) != VK_SUCCESS)return false;

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.rdvkbdevice.device, mvkobjs.rdvkbswapchain.swapchain, UINT64_MAX, mvkobjs.rdpresentsemaphore, VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreateswapchain();
	}
	else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}



    VkClearValue colorclearvalue;
    colorclearvalue.color = { {0.0f,0.0f,0.0f,0.0f } };

	VkClearValue depthvalue;
	depthvalue.depthStencil.depth = 1.0f;

	VkClearValue clearvals[] = { colorclearvalue,depthvalue };


	VkRenderPassBeginInfo rpinfo{};
	rpinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpinfo.renderPass = mvkobjs.rdrenderpass;

	rpinfo.renderArea.offset.x = 0;
	rpinfo.renderArea.offset.y = 0;
	rpinfo.renderArea.extent = mvkobjs.rdvkbswapchain.extent;
	rpinfo.framebuffer = mvkobjs.rdframebuffers[imgidx];

	rpinfo.clearValueCount = 2;
	rpinfo.pClearValues = clearvals;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
	viewport.width = static_cast<float>(mvkobjs.rdvkbswapchain.extent.width);
	viewport.height = -static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = mvkobjs.rdvkbswapchain.extent;




	if (vkResetCommandBuffer(mvkobjs.rdcommandbuffer[1], 0) != VK_SUCCESS)return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[1], &cmdbgninfo) != VK_SUCCESS)return false;



	vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer[1], &rpinfo, VK_SUBPASS_CONTENTS_INLINE);





	vkCmdSetViewport(mvkobjs.rdcommandbuffer[1], 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.rdcommandbuffer[1], 0, 1, &scissor);



	rdscene = mui.createloadingscreen(mvkobjs);
	mui.render(mvkobjs, mvkobjs.rdcommandbuffer[1]);




	vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[1]);

	if (vkEndCommandBuffer(mvkobjs.rdcommandbuffer[1]) != VK_SUCCESS)return false;




	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;

	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &mvkobjs.rdpresentsemaphore;

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rdrendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.rdcommandbuffer[1];


mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence) != VK_SUCCESS) {
		return false;
	}
mvkobjs.mtx2->unlock();

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rdrendersemaphore;

	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &mvkobjs.rdvkbswapchain.swapchain;

	presentinfo.pImageIndices = &imgidx;

mvkobjs.mtx2->lock();
	res = vkQueuePresentKHR(mvkobjs.rdpresentqueue, &presentinfo);
mvkobjs.mtx2->unlock();
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain();
	}
	else {
		if (res != VK_SUCCESS) {
			return false;
		}
	}
	return rdscene;
}

bool vkrenderer::drawblank(){
	if (vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		return false;
	}
	if (vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence) != VK_SUCCESS)return false;

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.rdvkbdevice.device, mvkobjs.rdvkbswapchain.swapchain, UINT64_MAX, mvkobjs.rdpresentsemaphore, VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return recreateswapchain();
	}
	else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
			return false;
		}
	}



    VkClearValue colorclearvalue;
    colorclearvalue.color = { {0.0f,0.0f,0.0f,0.0f } };

	VkClearValue depthvalue;
	depthvalue.depthStencil.depth = 1.0f;

	VkClearValue clearvals[] = { colorclearvalue,depthvalue };


	VkRenderPassBeginInfo rpinfo{};
	rpinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpinfo.renderPass = mvkobjs.rdrenderpass;

	rpinfo.renderArea.offset.x = 0;
	rpinfo.renderArea.offset.y = 0;
	rpinfo.renderArea.extent = mvkobjs.rdvkbswapchain.extent;
	rpinfo.framebuffer = mvkobjs.rdframebuffers[imgidx];

	rpinfo.clearValueCount = 2;
	rpinfo.pClearValues = clearvals;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
	viewport.width = static_cast<float>(mvkobjs.rdvkbswapchain.extent.width);
	viewport.height = -static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = mvkobjs.rdvkbswapchain.extent;



	if (vkResetCommandBuffer(mvkobjs.rdcommandbuffer[0], 0) != VK_SUCCESS)return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[0], &cmdbgninfo) != VK_SUCCESS)return false;



	vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer[0], &rpinfo, VK_SUBPASS_CONTENTS_INLINE);




	vkCmdSetViewport(mvkobjs.rdcommandbuffer[0], 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.rdcommandbuffer[0], 0, 1, &scissor);






	vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[0]);


	if (vkEndCommandBuffer(mvkobjs.rdcommandbuffer[0]) != VK_SUCCESS)return false;




	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;

	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &mvkobjs.rdpresentsemaphore;

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rdrendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.rdcommandbuffer[0];


    mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence) != VK_SUCCESS) {
		return false;
	}
    mvkobjs.mtx2->unlock();

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rdrendersemaphore;

	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &mvkobjs.rdvkbswapchain.swapchain;

	presentinfo.pImageIndices = &imgidx;

    mvkobjs.mtx2->lock();
	res = vkQueuePresentKHR(mvkobjs.rdpresentqueue, &presentinfo);

    mvkobjs.mtx2->unlock();

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain();
	}
	else {
		if (res != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}


