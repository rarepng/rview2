#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION


//#define _DEBUG

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <ctime>
#include <cstdlib>
#include <glm/gtx/spline.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <vk/vk_mem_alloc.h>
#include <iostream>
#include "vkrenderer.hpp"
#include "vksyncobjects.hpp"
//#ifdef _DEBUG
#include "logger.hpp"
//#endif

float map2(glm::vec3 x) {
	return std::max(x.y, 0.0f);
}
vkrenderer::vkrenderer(GLFWwindow* wind,GLFWmonitor* mont,const GLFWvidmode* mode) {
	mvkobjs.rdwind = wind;
	mvkobjs.rdmonitor = mont;
	mvkobjs.rdmode = mode;
	mvkobjs.decaying = &mspells[1]->active;


	mpersviewmats.emplace_back(glm::mat4{ 1.0f });
	mpersviewmats.emplace_back(glm::mat4{ 1.0f });
}
bool vkrenderer::init() {

	

	std::srand(static_cast<int>(time(NULL)));
	mvkobjs.rdheight = mvkobjs.rdvkbswapchain.extent.height;
	mvkobjs.rdwidth = mvkobjs.rdvkbswapchain.extent.width;
	if (!mvkobjs.rdwind)return false;
	if (!deviceinit()){
		return false;
	}
	if (!initvma()){
		return false;
	}
	if (!getqueue()){
		return false;
	}
	if (!createswapchain()){
		return false;
	}
	if (!createdepthbuffer()){
		return false;
	}
	if (!createcommandpool()){
		return false;
	}
	if (!createcommandbuffer()){
		return false;
	}
	if (!createrenderpass()){
		return false;
	}
	if (!createframebuffer()){
		return false;
	}
	if (!createsyncobjects()){
		return false;
	}
	if (!loadbackground()){
		return false;
	}
	if (!initui()){
		return false;
	}





	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;


	mframetimer.start();


	return true;
}
bool vkrenderer::initscene() {
	gamestate::init(mvkobjs, mnobjs, [&]() { moveplayer(); }, [&]() { moveenemies(); }, mplayer, mcircle, mpgltf, mspells, mplates, motherplayers);

	mpgltf.reserve(animfname.size());
	mpgltf.resize(animfname.size());
	mstatic0.reserve(staticfname.size());
	mstatic0.resize(staticfname.size());


	mground = std::make_shared<playoutground>();
	if (!mground->setup(mvkobjs, groundfname, groundobjs))return false;
	mcircle = std::make_shared<playoutcircle>();
	if (!mcircle->setup(mvkobjs, dndcount))return false;
	mplates = std::make_shared<playoutcircle>();
	if (!mplates->setup(mvkobjs, totalanimcounts))return false;

	mvkobjs.loadingprog += 0.1f;


	if(mnobjs.offlineplay)
		mplayer = std::make_shared<playoutplayer>();
	else if(mnobjs.rdserverclient) {
		mplayer = motherplayers[0];
		gamestate::aposes[0] = glm::vec3{ 0.0f };
	} else {
		mplayer = motherplayers[0];///////////////////////////////////////////////////todo
		gamestate::aposes[0] = glm::vec3{ 0.0f };
	}
	if (!mplayer->setup(mvkobjs, playerfname, playercount))return false;

	mvkobjs.loadingprog += 0.1f;

	for (size_t i{ 0 }; i < mpgltf.size(); i++) {
		mpgltf[i] = std::make_shared<playoutmodel>();
		if (!mpgltf[i]->setup(mvkobjs, animfname[i], animcounts[i]))return false;
	}

	mvkobjs.loadingprog += 0.1f;

	for (size_t i{ 0 }; i < mstatic0.size(); i++) {
		mstatic0[i] = std::make_shared<playoutstatic>();
		if (!mstatic0[i]->setup(mvkobjs, staticfname[i], staticcounts[i]))return false;
	}

	mvkobjs.loadingprog += 0.1f;

	lifebar = std::make_shared<playoutmenubg>();
	if (!lifebar->setup(mvkobjs, 1))return false;

	mvkobjs.loadingprog += 0.1f;

	if (!mground->setup2(mvkobjs, groundshaders[0], groundshaders[1]))return false;
	if (!mcircle->setup2(mvkobjs,circletexture, dndshaders[0], dndshaders[1]))return false;
	if (!mplates->setup2(mvkobjs,hptexture, plateshaders[0], plateshaders[1]))return false;

	mvkobjs.loadingprog += 0.1f;

	if (!mplayer->setup2(mvkobjs, playershaders[0], playershaders[1]))return false;

	mvkobjs.loadingprog += 0.1f;

	for (size_t i{ 0 }; i < mpgltf.size(); i++) {
		if (!mpgltf[i]->setup2(mvkobjs, animshaders[0], animshaders[1]))return false;
	}

	mvkobjs.loadingprog += 0.1f;

	for (size_t i{ 0 }; i < mstatic0.size(); i++) {
		if (!mstatic0[i]->setup2(mvkobjs, staticshaders[0], staticshaders[1]))return false;
	}

	mvkobjs.loadingprog += 0.1f;

	if (!lifebar->setup2(mvkobjs, lifebarshaders[0], lifebarshaders[1]))return false;

	mvkobjs.loadingprog += 0.1f;

	gamestate::setstate(gamestate0::normal);

	return true;
}
void vkrenderer::initshop(){
	mchoices.reserve(1);
	mchoices.resize(1);
	mstaticchoices.reserve(staticshopfname.size());
	mstaticchoices.resize(staticshopfname.size());

	mchoices[0] = std::make_shared<playoutcircle>();
	mchoices[0]->setup(mvkobjs, 2);
	mchoices[0]->setup2(mvkobjs,shopbacktextures[0], shopbgshaders[0], shopbgshaders[1]);

	mstaticchoices.front() = std::make_shared<playoutstatic>();
	mstaticchoices.back() = std::make_shared<playoutstatic>();
	mstaticchoices.front()->setup(mvkobjs, staticshopfname.front(), shopcounts.front());
	mstaticchoices.back()->setup(mvkobjs, staticshopfname.back(), shopcounts.back());
	mstaticchoices.front()->setup2(mvkobjs, staticshopshaders[0], staticshopshaders[1]);
	mstaticchoices.back()->setup2(mvkobjs, staticshopshaders[0], staticshopshaders[1]);
	//for (size_t i{ 0 }; i < mchoices.size(); i++) {
	//	for (const auto& j : mchoices[i]->getallinstances()) {
	//	}
	//}
	//for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
	//for (const auto& j : mstaticchoices[0]->getallinstances()) {
	//	j->getinstancesettings().msworldpos = glm::vec3(static_cast<float>(std::rand() % 100) / 200.0f - 1.0f, static_cast<float>(std::rand() % 100) / 120.0f - 1.0f, 0.5f);
	//	j->getinstancesettings().msworldrot = glm::vec3(static_cast<float>(std::rand() % 360), static_cast<float>(std::rand() % 360), static_cast<float>(std::rand() % 360));
	//	j->getinstancesettings().msworldscale = glm::vec3(0.2f, 0.2f, 0.2f);
	//}
	for (const auto& j : mstaticchoices[0]->getallinstances()) {
		j->getinstancesettings().msworldpos =glm::vec3(-0.5f, 0.0f, 0.4f);
		j->getinstancesettings().msworldrot = glm::vec3(-90.0f,0.0f, -90.0f);
		j->getinstancesettings().msworldscale = glm::vec3(0.4f, 0.4f, 0.4f);
	}
	for (const auto& j : mstaticchoices[1]->getallinstances()) {
		j->getinstancesettings().msworldpos = glm::vec3( 0.5f, 0.0f , 0.4f);
		j->getinstancesettings().msworldrot = glm::vec3(-90.0f, 0.0f, 0.0f);
		j->getinstancesettings().msworldscale = glm::vec3(0.2f, 0.2f, 0.2f);
	}
	//}

}
bool vkrenderer::getserverclientstatus(){
	return mnobjs.rdserverclient;
}
ui* vkrenderer::getuihandle(){
	return &mui;
}
netobjs& vkrenderer::getnetobjs() {
	return mnobjs;
}
vkobjs& vkrenderer::getvkobjs() {
	return mvkobjs;
}
bool vkrenderer::quicksetup(netclient* nclient){
	mnobjs.nclient = nclient;
	ImGui_ImplGlfw_RestoreCallbacks(mvkobjs.rdwind);
	glfwSetKeyCallback(mvkobjs.rdwind, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handlekey(key, scancode, action, mods);
	});
	ImGui_ImplGlfw_InstallCallbacks(mvkobjs.rdwind);
	//ImGui_ImplGlfw_RestoreCallbacks(mvkobjs.rdwind);
	playerlocation = mplayer->getinst(0)->getinstpos();
	inmenu = false;


	modelsettings& s = mplayer->getinst(0)->getinstancesettings();
	//s.msworldscale = glm::vec3{ 60.0f };

	playerhp = &s.hp;


	//offline
	mui.playerwave.push_back(1);

	std::for_each(mplates->getallinstances().begin(), mplates->getallinstances().end(), [&](std::shared_ptr<texinstance>& x) { x->getinstancesettings().msworldpos.y = 200; });


	return true;
}
bool vkrenderer::quicksetup(netserver* nserver) {
	mnobjs.nserver = nserver;
	ImGui_ImplGlfw_RestoreCallbacks(mvkobjs.rdwind);
	glfwSetKeyCallback(mvkobjs.rdwind, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handlekey(key, scancode, action, mods);
	});
	ImGui_ImplGlfw_InstallCallbacks(mvkobjs.rdwind);
	//ImGui_ImplGlfw_RestoreCallbacks(mvkobjs.rdwind);
	playerlocation = mplayer->getinst(0)->getinstpos();
	inmenu = false;
	modelsettings& s = mplayer->getinst(0)->getinstancesettings();
	//s.msworldscale = glm::vec3{ 60.0f };

	playerhp = &s.hp;


	std::for_each(mplates->getallinstances().begin(), mplates->getallinstances().end(), [&](std::shared_ptr<texinstance>& x) { x->getinstancesettings().msworldpos.y = 200; });


	return true;
}
void vkrenderer::wavesetup(){

	size_t k = 0;
	for (size_t i{ 0 }; i < mpgltf.size(); i++) {
		for (size_t j{ 0 }; j < mpgltf[i]->getnuminstances(); j++) {
			staticsettings& s = mplates->getinst(k++)->getinstancesettings();
			//s.msworldpos.x = mpgltf[i]->getinst(j)->getinstancesettings().msworldpos.x + glm::vec3{ 0.0f,200.0f,0.0f };
			s.msworldpos.x = mpgltf[i]->getinst(j)->getinstancesettings().msworldpos.x;
			s.msworldpos.z = mpgltf[i]->getinst(j)->getinstancesettings().msworldpos.z;
			s.msworldscale = glm::vec3{ 100.0f,28.0f,100.0f };
			//s.msworldrot = mpgltf[i]->getinst(j)->getinstancesettings().msworldrot;
			glm::vec3 diff = glm::normalize(mvkobjs.rdcamwpos - s.msworldpos);
			s.msworldrot.y = glm::degrees(glm::atan(diff.x,diff.z));
		}
	}

}
bool vkrenderer::loadbackground(){
	mbackground = std::make_shared<playoutback>();
	mmenubg = std::make_shared<playoutmenubg>();
	if (!mbackground->setup(mvkobjs, backfname, backgobjs)){
		return false;
	}
	if (!mbackground->setup2(mvkobjs, backshaders[0], backshaders[1])){
		return false;
	}
	if (!mmenubg->setup(mvkobjs,1)){
		return false;
	}
	if (!mmenubg->setup2(mvkobjs, menubgshaders[0], menubgshaders[1])){
		return false;
	}
	return true;
}
bool vkrenderer::deviceinit() {
	vkb::InstanceBuilder instbuild{};

	//std::lock_guard<std::shared_mutex> lg{ *mvkobjs.mtx2 };
	auto instret = instbuild.use_default_debug_messenger().request_validation_layers().require_api_version(1, 2, 0).build();
	

	//instret.value().

	mvkobjs.rdvkbinstance = instret.value();

	VkResult res = VK_ERROR_UNKNOWN;
	res = glfwCreateWindowSurface(mvkobjs.rdvkbinstance, mvkobjs.rdwind, nullptr, &msurface);

	

	vkb::PhysicalDeviceSelector physicaldevsel{ mvkobjs.rdvkbinstance };
	auto firstphysicaldevselret = physicaldevsel.set_surface(msurface).set_minimum_version(1,2).select();

	//VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT x;
	//x.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
	//x.swapchainMaintenance1 = VK_FALSE;
	//x.pNext = VK_NULL_HANDLE;

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






	//auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 3).set_surface(msurface).set_required_features(physfeatures.features).add_required_extension_features(physmeshfeatures).set_required_features_12(physfeatures12).set_required_features_13(physfeatures13).add_required_extension("VK_EXT_mesh_shader").select();
	auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 2).set_surface(msurface).set_required_features(physfeatures.features).set_required_features_11(physfeatures11).set_required_features_12(physfeatures12).set_required_features_13(physfeatures13).select();
	//auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1, 0).set_surface(msurface).select();

	//std::cout << "\n\n\n\n\n\n\n\n\n\n mesh shader value: " << secondphysicaldevselret.value().is_extension_present("VK_EXT_mesh_shader") << "\n\n\n\n\n";
	//std::cout << "\n\n\n\n\n\n\n\n\n\n maintenance: " << secondphysicaldevselret.value().is_extension_present("VK_EXT_swapchain_maintenance1") << "\n\n\n\n\n";
	mvkobjs.rdvkbphysdev = secondphysicaldevselret.value();


	//for (auto& x : secondphysicaldevselret.value().get_available_extensions()) { std::cout << x << std::endl; }

	mminuniformbufferoffsetalignment = mvkobjs.rdvkbphysdev.properties.limits.minUniformBufferOffsetAlignment;

	vkb::DeviceBuilder devbuilder{ mvkobjs.rdvkbphysdev };
	auto devbuilderret = devbuilder.build();
	mvkobjs.rdvkbdevice = devbuilderret.value();

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
	auto swapchainbuilret = swapchainbuild.set_old_swapchain(mvkobjs.rdvkbswapchain).set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR).build();
	if (!swapchainbuilret) {
		return false;
	}
	vkb::destroy_swapchain(mvkobjs.rdvkbswapchain);
	mvkobjs.rdvkbswapchain = swapchainbuilret.value();
	return true;
}
bool vkrenderer::recreateswapchain() {
	glfwGetFramebufferSize(mvkobjs.rdwind, &mvkobjs.rdwidth, &mvkobjs.rdheight);
	while (mvkobjs.rdwidth == 0 || mvkobjs.rdheight == 0) {
		glfwGetFramebufferSize(mvkobjs.rdwind, &mvkobjs.rdwidth, &mvkobjs.rdheight);
		glfwWaitEvents();
	}
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

	for (size_t i{ 0 }; i < mpgltf.size(); i++) {
		mpgltf[i]->cleanupmodels(mvkobjs);
	}

	for (size_t i{ 0 }; i < mstatic0.size(); i++) {
		mstatic0[i]->cleanupmodels(mvkobjs);
	}
	if (!mnobjs.offlineplay) {
		for (const auto& i : motherplayers) {
			if(i.second->ready)
			i.second->cleanupmodels(mvkobjs);
		}
	}else if (mplayer)
		mplayer->cleanupmodels(mvkobjs);
	if (mground)
		mground->cleanupmodels(mvkobjs);

	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		mchoices[i]->cleanupmodels(mvkobjs);
	}
	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		mstaticchoices[i]->cleanupmodels(mvkobjs);
	}


	if (mcircle)mcircle->cleanupmodels(mvkobjs);
	if (mplates)mplates->cleanupmodels(mvkobjs);


	if(lifebar)
	lifebar->cleanupmodels(mvkobjs);


	mui.cleanup(mvkobjs);


	//cleanmainmenu();
	//cleanloading();

	vksyncobjects::cleanup(mvkobjs);
	commandbuffer::cleanup(mvkobjs, mvkobjs.rdcommandpool[0], mvkobjs.rdcommandbuffer[0]);
	commandpool::cleanup(mvkobjs, mvkobjs.rdcommandpool[0]);
	commandpool::cleanup(mvkobjs, mvkobjs.rdcommandpool[2]);
	framebuffer::cleanup(mvkobjs);


	for (size_t i{ 0 }; i < mpgltf.size(); i++) {
		mpgltf[i]->cleanuplines(mvkobjs);
	}

	for (size_t i{ 0 }; i < mstatic0.size(); i++) {
		mstatic0[i]->cleanuplines(mvkobjs);
	}

	if (!mnobjs.offlineplay) {
		for (const auto& i : motherplayers) {
			if (i.second->ready)
			i.second->cleanuplines(mvkobjs);
		}
	}else if (mplayer)
		mplayer->cleanuplines(mvkobjs);
	if (mground)
		mground->cleanuplines(mvkobjs);
	if(lifebar)
	lifebar->cleanuplines(mvkobjs);

	if (mcircle)mcircle->cleanuplines(mvkobjs);
	if (mplates)mplates->cleanuplines(mvkobjs);



	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		mchoices[i]->cleanuplines(mvkobjs);
	}
	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		mstaticchoices[i]->cleanuplines(mvkobjs);
	}




	renderpass::cleanup(mvkobjs);

	for (size_t i{ 0 }; i < mpgltf.size(); i++) {
		mpgltf[i]->cleanupbuffers(mvkobjs);
	}

	for (size_t i{ 0 }; i < mstatic0.size(); i++) {
		mstatic0[i]->cleanupbuffers(mvkobjs);
	}

	if (!mnobjs.offlineplay) {
		for (const auto& i : motherplayers) {
			if (i.second->ready)
				i.second->cleanupbuffers(mvkobjs);
		}
	}else if (mplayer)
		mplayer->cleanupbuffers(mvkobjs);
	if (mground)
		mground->cleanupbuffers(mvkobjs);

	if (mcircle)mcircle->cleanupbuffers(mvkobjs);
	if (mplates)mplates->cleanupbuffers(mvkobjs);




	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		mchoices[i]->cleanupbuffers(mvkobjs);
	}
	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		mstaticchoices[i]->cleanupbuffers(mvkobjs);
	}



	vkDestroyImageView(mvkobjs.rdvkbdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.rdallocator, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);
	vmaDestroyAllocator(mvkobjs.rdallocator);

	mvkobjs.rdvkbswapchain.destroy_image_views(mvkobjs.rdswapchainimageviews);
	vkb::destroy_swapchain(mvkobjs.rdvkbswapchain);

	vkb::destroy_device(mvkobjs.rdvkbdevice);
	vkb::destroy_surface(mvkobjs.rdvkbinstance.instance, msurface);
	vkb::destroy_instance(mvkobjs.rdvkbinstance);


}
void vkrenderer::cleanloading() {
	//commandbuffer::cleanup(mvkobjs, mvkobjs.rdcommandpool[1], mvkobjs.rdcommandbuffer[1]);
	//commandpool::cleanup(mvkobjs, mvkobjs.rdcommandpool[1]);
}
void vkrenderer::cleanmainmenu() {
	mbackground->cleanupmodels(mvkobjs);
	mbackground->cleanupbuffers(mvkobjs);
	mbackground->cleanuplines(mvkobjs);
	mmenubg->cleanupmodels(mvkobjs);
	mmenubg->cleanuplines(mvkobjs);
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

	
	if (!mnobjs.offlineplay) {
		for (const auto& i : motherplayers) {
			if(i.second->ready)
			i.second->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);
		}
	} else {
		mplayer->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);
	}
	for (size_t i{ 0 }; i < mpgltf.size(); i++) {
		mpgltf[i]->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);
	}

	for (size_t i{ 0 }; i < mstatic0.size(); i++) {
		mstatic0[i]->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);
	}
	mground->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);

	if (mspells[0]->active)mcircle->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);

	mplates->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);

	lifebar->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);

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
	vkQueuePresentKHR(mvkobjs.rdpresentqueue, &presentinfo);

	mvkobjs.mtx2->unlock();

	//mvkobjs.uploadmtx->unlock();
	return true;
}

void vkrenderer::uploadforshop(){
	vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX);
	vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence);

	uint32_t imgidx = 0;

	VkResult res = vkAcquireNextImageKHR(mvkobjs.rdvkbdevice.device, mvkobjs.rdvkbswapchain.swapchain, UINT64_MAX, mvkobjs.rdpresentsemaphore, VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateswapchain();
	}
	vkResetCommandBuffer(mvkobjs.rdcommandbuffer[0], 0);

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[0], &cmdbgninfo);


	//upload


	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		mchoices[i]->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);
	}
	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		mstaticchoices[i]->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);
	}




	vkEndCommandBuffer(mvkobjs.rdcommandbuffer[0]);


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
	vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence);
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
		recreateswapchain();
	} 





}

void vkrenderer::handlekeymenu(int key, int scancode, int action, int mods) {

	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_F4) == GLFW_PRESS) {
		if (mvkobjs.rdfullscreen)glfwSetWindowMonitor(mvkobjs.rdwind, nullptr, 100, 200, 900, 600, GLFW_DONT_CARE);
		else {
			glfwSetWindowMonitor(mvkobjs.rdwind, mvkobjs.rdmonitor, 0, 0, mvkobjs.rdmode->width, mvkobjs.rdmode->height, mvkobjs.rdmode->refreshRate);
		}
		mvkobjs.rdfullscreen = !mvkobjs.rdfullscreen;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_F3) == GLFW_PRESS) {
		mui.setnetwork = !mui.setnetwork;
	}

	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(mvkobjs.rdwind, true);
	}
	//if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_ENTER) == GLFW_PRESS) {
	//	ImGui_ImplGlfw_InstallCallbacks(mvkobjs.rdwind);
	//}

}
void vkrenderer::handlekey(int key, int scancode, int action, int mods){
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_SPACE) == GLFW_PRESS) {
		switchshader = !switchshader;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
		mui.backspace();
	}
	//if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_ENTER) == GLFW_PRESS) {
	//}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_F4) == GLFW_PRESS) {
		if (mvkobjs.rdfullscreen)glfwSetWindowMonitor(mvkobjs.rdwind, nullptr, 100, 200, 900, 600, GLFW_DONT_CARE);
		else {
			glfwSetWindowMonitor(mvkobjs.rdwind, mvkobjs.rdmonitor, 0, 0, mvkobjs.rdmode->width, mvkobjs.rdmode->height, mvkobjs.rdmode->refreshRate);
		}
		mvkobjs.rdfullscreen = !mvkobjs.rdfullscreen;
	}


	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		if (gamestate::getstate() == gamestate0::menu) {
			gamestate::setstate(gamestate0::normal);
		}
		if (gamestate::getpause()==pausestate::paused) {
			gamestate::setpause(pausestate::resumed);
		} else {
			gamestate::setpause(pausestate::paused);
			pausebgntime = glfwGetTime();
		}
		//glfwSetWindowShouldClose(mvkobjs.rdwind, true);
	}

	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_F) == GLFW_PRESS) {
		if (mspells[0]->ready) {
			double x;
			double y;
			glfwGetCursorPos(mvkobjs.rdwind, &x, &y);
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
				mspells[0]->cast = true;
				mspells[0]->ready = false;
				mvkobjs.raymarchpos = mvkobjs.rdcamwpos + cfor * d;
			}
		}
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_R) == GLFW_PRESS) {
		if (mspells[1]->ready) {
			decaystart = glfwGetTime();
			{
				double x;
				double y;
				glfwGetCursorPos(mvkobjs.rdwind, &x, &y);
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
					//playerlookto = glm::normalize(mvkobjs.rdcamwpos + cfor * d - s.msworldpos);
					//movediff = glm::vec2(glm::abs(glm::vec3((mvkobjs.rdcamwpos + cfor * d) - s.msworldpos)).x, glm::abs(glm::vec3((mvkobjs.rdcamwpos + cfor * d) - s.msworldpos)).z);

					//if (movediff.x > 2.1f || movediff.y > 2.1f) {
						mvkobjs.raymarchpos = mvkobjs.rdcamwpos + cfor * d;
						mspells[1]->cast = true;
						mspells[1]->ready = false;
						decaypos = mplayer->getinst(0)->getinstancesettings().msworldpos;

						//s.msworldrot.y = glm::degrees(glm::atan(playerlookto.x, playerlookto.z));



						playermoving = false;
				}
			}
		}
	}

}
void vkrenderer::moveplayer(){
	if (playermoving) {
		modelsettings& s = mplayer->getinst(0)->getinstancesettings();
		glm::vec3 diff = playermoveto - s.msworldpos;
		s.msworldpos += glm::normalize(diff) * 2.0f;
		if (glm::abs(diff.x) < 2.1f && glm::abs(diff.z) < 2.1f) {
			s.msworldpos = playermoveto;
			s.msanimclip = 2;
			playermoving = false;
		}
	}
}
void vkrenderer::moveenemies(){
	//if (!mspells[1]->active) {

		int k = 0;
		for (int i{ 0 }; i < mpgltf.size(); i++) {
			for (int j{ 0 }; j < mpgltf[i]->getnuminstances(); j++, k++) {
				modelsettings& es = mpgltf[i]->getinst(j)->getinstancesettings();
				if (es.dead) {
					mplates->getinst(k)->getinstancesettings().msworldpos.y = -200.0;
					continue;
				}
				if (es.hp > 0.0) {
					glm::vec3 diff = *playerlocation - es.msworldpos;
					if (glm::abs(diff.x) < 100.0f && glm::abs(diff.z) < 100.0f) {
						if (es.msanimclip > 0) {
							es.msanimclip = 0;
							es.msanimtimepos = 0.0f;
						}
						if (es.msanimtimepos > 1.2f) {
							*playerhp -= 0.001f;
						}
					} else {
						es.msworldpos += glm::normalize(diff) * 1.0f;
						es.msanimclip = 3;
						es.msworldrot.y = glm::degrees(glm::atan(diff.x, diff.z));
					}
				}
			}
		}
	//} 
	//else {
	//	int k = 0;
	//	for (int i{ 0 }; i < mpgltf.size(); i++) {
	//		for (int j{ 0 }; j < mpgltf[i]->getnuminstances(); j++, k++) {
	//			modelsettings& es = mpgltf[i]->getinst(j)->getinstancesettings();
	//			if (es.dead)continue;
	//			glm::vec3 diff = decaypos - es.msworldpos;
	//			if (glm::abs(diff.x) < 100.0f && glm::abs(diff.z) < 100.0f) {
	//				if (es.msanimclip > 0) {
	//					es.msanimclip = 0;
	//					es.msanimtimepos = 0.0f;
	//				}
	//			} else {
	//				es.msworldpos += glm::normalize(diff) * 1.0f;
	//				es.msanimclip = 3;
	//				es.msworldrot.y = glm::degrees(glm::atan(diff.x, diff.z));
	//			}
	//		}
	//	}
	//}
}
void vkrenderer::handleclick(int key, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (key >= 0 && key < ImGuiMouseButton_COUNT) {
		io.AddMouseButtonEvent(key, action == GLFW_PRESS);
	}
	if (io.WantCaptureMouse)return;
	


	if (key == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		mlock = !mlock;
		if (mlock) {
			glfwSetInputMode(mvkobjs.rdwind, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(mvkobjs.rdwind, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			}
		} else {
			glfwSetInputMode(mvkobjs.rdwind, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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


	if (gamestate::getstate() == gamestate0::normal) {
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

	if (gamestate::getstate() == gamestate0::menu && glfwGetMouseButton(mvkobjs.rdwind,GLFW_MOUSE_BUTTON_LEFT) != GLFW_RELEASE) {
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
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_W) == GLFW_PRESS) {
		mvkobjs.rdcamforward += 200;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_S) == GLFW_PRESS) {
		mvkobjs.rdcamforward -= 200;
	}
	mvkobjs.rdcamright = 0;
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_A) == GLFW_PRESS) {
		mvkobjs.rdcamright += 200;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_D) == GLFW_PRESS) {
		mvkobjs.rdcamright -= 200;
	}
	mvkobjs.rdcamup = 0;
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_E) == GLFW_PRESS) {
		mvkobjs.rdcamup += 200;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_Q) == GLFW_PRESS) {
		mvkobjs.rdcamup -= 200;
	}

	if (glfwGetMouseButton(mvkobjs.rdwind, GLFW_MOUSE_BUTTON_LEFT) != GLFW_RELEASE) {
		if (gamestate::getstate() == gamestate0::normal) {
			double x;
			double y;
			glfwGetCursorPos(mvkobjs.rdwind, &x, &y);
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
				modelsettings& s = mplayer->getinst(0)->getinstancesettings();

				playerlookto = glm::normalize(mvkobjs.rdcamwpos + cfor * d - s.msworldpos);
				movediff = glm::vec2(glm::abs(glm::vec3((mvkobjs.rdcamwpos + cfor * d) - s.msworldpos)).x, glm::abs(glm::vec3((mvkobjs.rdcamwpos + cfor * d) - s.msworldpos)).z);

				if (movediff.x>2.1f||movediff.y>2.1f) {
					playermoveto = mvkobjs.rdcamwpos + cfor * d;
					mvkobjs.raymarchpos = mvkobjs.rdcamwpos + cfor * d;

					s.msworldrot.y = glm::degrees(glm::atan(playerlookto.x, playerlookto.z));

					s.msanimclip = 3;

					playermoving = true;
				}
			}
		}
	}


}



void vkrenderer::checkforanimupdates() {

	while (!glfwWindowShouldClose(mvkobjs.rdwind)) {
		if (gamestate::getstate() == gamestate0::dead) break;

		if (gamestate::getpause() == pausestate::resumed && gamestate::getstate() == gamestate0::normal) {

			muidrawtimer.start();
			//updatemtx.lock();
			
			mplayer->getinst(0)->checkforupdates();
			for (const auto& i : mpgltf) {
				for (const auto& j : i->getallinstances()) {
					j->checkforupdates();
				}
			}
			for (const auto& i : mplates->getallinstances()) {
				i->checkforupdates();
			}
			if (mspells[0]->active)mcircle->getinst(0)->checkforupdates();

			//animmtx.lock();
			//updatemtx.unlock();
			//animmtx.unlock();
			//std::this_thread::sleep_for(std::chrono::milliseconds(200));
			mvkobjs.rduidrawtime = muidrawtimer.stop();
		}
	}

}

void vkrenderer::updateanims(){


	while (!glfwWindowShouldClose(mvkobjs.rdwind)) {
		if (gamestate::getstate() == gamestate0::dead) break;

		if (gamestate::getpause() == pausestate::resumed && gamestate::getstate() == gamestate0::normal) {


				manimupdatetimer.start();

				//animmtx.lock();
				mplayer->updateanims();
				for (size_t i{ 0 }; i < mpgltf.size(); i++) {
					mpgltf[i]->updateanims();
				}
				//updatemtx.lock();
				//animmtx.unlock();
				//updatemtx.unlock();
				//std::this_thread::sleep_for(std::chrono::milliseconds(200));

				mvkobjs.updateanimtime = manimupdatetimer.stop();



		}
	}
}

void vkrenderer::animateshop(){
		for (const auto& j : mstaticchoices[0]->getallinstances()) {
			j->getinstancesettings().msworldrot.x += 0.4f;
		}
		for (const auto& j : mstaticchoices[1]->getallinstances()) {
			j->getinstancesettings().msworldrot.y += 0.2f;
		}
}

void vkrenderer::gametick() {
	while (!glfwWindowShouldClose(mvkobjs.rdwind)) {

		if (gamestate::getstate() == gamestate0::dead) break;

		if(gamestate::getpause() == pausestate::resumed){
			if(gamestate::getstate()== gamestate0::normal)
				gamestate::tick();
		}

	}
}

bool vkrenderer::draw() {

	if (newconnection) {
		newconnection = false;
		uploadfordraw();
	}

	if (gamestate::getpause() == pausestate::resumed) {




		double tick = glfwGetTime();
		mvkobjs.tickdiff = tick - mlasttick;
		mvkobjs.frametime = mframetimer.stop();
		mframetimer.start();

		muigentimer.start();

		enemyhps.clear();
		//size_t k{ 0 };
		for (size_t i{ 0 }; i < mpgltf.size(); i++) {
			for (size_t j{ 0 }; j < mpgltf[i]->getnuminstances(); j++) {
				enemyhps.push_back(mpgltf[i]->getinst(j)->getinstancesettings().hp);
			}
			//k += mpgltf[i]->getnuminstances();
		}


		mvkobjs.rduigeneratetime = muigentimer.stop();





		//joint anims
		if (dummytick / 2) {

			///////////////////////////////////////////
			if(!mnobjs.offlineplay)
			for (const auto& i : motherplayers) {
				if(i.second->ready)
				i.second->updateanims();
			}
			mplayer->updateanims();
			for (size_t i{ 0 }; i < mpgltf.size(); i++) {
				mpgltf[i]->updateanims();
			}


			dummytick = 0;
		} 

		mmatupdatetimer.start();

		//non joint mats
		if (mspells[0]->active)mcircle->updatemats();

		mplates->updatemats();




		if (!mnobjs.offlineplay)
			for (const auto& i : motherplayers) {
				if (i.second->ready)
				i.second->updatemats();
			}



		//joint mats
		mplayer->updatemats();
		for (size_t i{ 0 }; i < mpgltf.size(); i++) {
			mpgltf[i]->updatemats();
		}

		mvkobjs.updatemattime = mmatupdatetimer.stop();
		
		//joint check
		if(dummytick%2){
			wavesetup();
			mplayer->getinst(0)->checkforupdates();
			for (const auto& i : mpgltf) {
				for (const auto& j : i->getallinstances()) {
					j->checkforupdates();
				}
			}

			if (!mnobjs.offlineplay)
				for (const auto& i : motherplayers) {
					if (i.second->ready)
					i.second->getinst(0)->checkforupdates();
				}

		}


		//non joint check
		for (const auto& i : mplates->getallinstances()) {
			i->checkforupdates();
		}
		if (mspells[0]->active)mcircle->getinst(0)->checkforupdates();




		dummytick++;







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
		colorclearvalue.color = { {0.12f,0.12f,0.12f,1.0f } };

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


		if (mspells[0]->active)mcircle->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);





		vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer[0], &rpinfo, VK_SUBPASS_CONTENTS_INLINE);





		vkCmdSetViewport(mvkobjs.rdcommandbuffer[0], 0, 1, &viewport);
		vkCmdSetScissor(mvkobjs.rdcommandbuffer[0], 0, 1, &scissor);

		mground->draw(mvkobjs);

		for (size_t i{ 0 }; i < mstatic0.size(); i++) {
			mstatic0[i]->draw(mvkobjs);
		}

		for (size_t i{ 0 }; i < mpgltf.size(); i++) {
			mpgltf[i]->draw(mvkobjs);
		}

		mplayer->draw(mvkobjs);


		if (!mnobjs.offlineplay)
			for (const auto& i : motherplayers) {
				if (i.second->ready)
				i.second->draw(mvkobjs);
			}



		if (mspells[1]->active) {
			decaytime = glfwGetTime() - decaystart-pausetime;
			mplayer->drawdecays(mvkobjs, decaytime, &mspells[1]->active);
		}
		if (!mspells[1]->active)pausetime = 0.0;


		lifetime = glfwGetTime();
		lifetime2 = glfwGetTime();

		lifebar->draw(mvkobjs,lifetime, lifetime2,*playerhp);

		if (mspells[0]->active)mcircle->draw(mvkobjs, lifetime,decaytime,*playerhp);

		mplates->draw(mvkobjs, lifetime,decaytime, *playerhp);

		modelsettings& settings = mplayer->getinst(0)->getinstancesettings();
		mui.createdbgframe(mvkobjs, settings, mnobjs);

		mui.render(mvkobjs, mvkobjs.rdcommandbuffer[0]);


		vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[0]);

		//animmtx.lock();
		//updatemtx.lock();

		muploadubossbotimer.start();

		mplayer->uploadubossbo(mvkobjs, mpersviewmats);

		if (!mnobjs.offlineplay)
			for (const auto& i : motherplayers) {
				if (i.second->ready)
				i.second->uploadubossbo(mvkobjs, mpersviewmats);
			}

		for (size_t i{ 0 }; i < mpgltf.size(); i++) {
			mpgltf[i]->uploadubossbo(mvkobjs, mpersviewmats);
		}

		for (size_t i{ 0 }; i < mstatic0.size(); i++) {
			mstatic0[i]->uploadubossbo(mvkobjs, mpersviewmats);
		}

		mground->uploadubossbo(mvkobjs, mpersviewmats);

		if (mspells[0]->active)mcircle->uploadubossbo(mvkobjs, mpersviewmats, enemyhps);

		mplates->uploadubossbo(mvkobjs, mpersviewmats, enemyhps);

		mvkobjs.uploadubossbotime = muploadubossbotimer.stop();
		//animmtx.unlock();
		//updatemtx.unlock();







		//VkRenderPassBeginInfo rpinfo2{};
		//rpinfo2.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		//rpinfo2.renderPass = mvkobjs.rdrenderpass2;
		//rpinfo2.renderArea.offset.x = 0;
		//rpinfo2.renderArea.offset.y = 0;
		//rpinfo2.renderArea.extent = mvkobjs.rdvkbswapchain.extent;
		//rpinfo2.framebuffer = mvkobjs.rdframebuffers[imgidx];
		//rpinfo2.clearValueCount = 2;
		//rpinfo2.pClearValues = clearvals;

		//vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer[0], &rpinfo2, VK_SUBPASS_CONTENTS_INLINE);





		//vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[0]);







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
	else {


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
			colorclearvalue.color = { {0.0048f,0.0048f,0.0048f,1.0f } };

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





			mground->draw(mvkobjs);

			for (size_t i{ 0 }; i < mstatic0.size(); i++) {
				mstatic0[i]->draw(mvkobjs);
			}

			for (size_t i{ 0 }; i < mpgltf.size(); i++) {
				mpgltf[i]->draw(mvkobjs);
			}

			mplayer->draw(mvkobjs);

			if (mspells[1]->active) {
				decaytime = glfwGetTime() - decaystart-pausetime;
				mplayer->drawdecays(mvkobjs, decaytime, &mspells[1]->active);
			}

			lifetime = glfwGetTime() - pausetime;
			lifetime2 = glfwGetTime();

			lifebar->draw(mvkobjs,lifetime, lifetime2,*playerhp);







			if(mui.createpausebuttons(mvkobjs))gamestate::setpause(pausestate::resumed);
			mui.render(mvkobjs, mvkobjs.rdcommandbuffer[0]);





			vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[0]);


			mplayer->uploadubossbo(mvkobjs, mpersviewmats);


			for (size_t i{ 0 }; i < mpgltf.size(); i++) {
				mpgltf[i]->uploadubossbo(mvkobjs, mpersviewmats);
			}

			for (size_t i{ 0 }; i < mstatic0.size(); i++) {
				mstatic0[i]->uploadubossbo(mvkobjs, mpersviewmats);
			}

			mground->uploadubossbo(mvkobjs, mpersviewmats);





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
			} else {
				if (res != VK_SUCCESS) {
					return false;
				}
			}
			pausetime = glfwGetTime() - pausebgntime;


			return true;
	}
}
bool vkrenderer::drawmainmenu() {

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
	colorclearvalue.color = { {0.012f,0.012f,0.012f,1.0f } };

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



	//mbackground->updateanims();


	if (vkResetCommandBuffer(mvkobjs.rdcommandbuffer[0], 0) != VK_SUCCESS)return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[0], &cmdbgninfo) != VK_SUCCESS)return false;

	mbackground->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);
	mmenubg->uploadvboebo(mvkobjs, mvkobjs.rdcommandbuffer[0]);

	//staticsettings s{};
	mbackground->getinst(0)->getinstancesettings().msworldrot.y += 0.022f;
	//s.msworldpos.y = -20.0f;
	//s.msworldrot.y -= 1.57f;
	
	//mbackground->getinst(0)->setinstancesettings(s);
	mbackground->updatemats();

	vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer[0], &rpinfo, VK_SUBPASS_CONTENTS_INLINE);




	vkCmdSetViewport(mvkobjs.rdcommandbuffer[0], 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.rdcommandbuffer[0], 0, 1, &scissor);


	mbackground->draw(mvkobjs);

	lifetime = glfwGetTime();

	mmenubg->draw(mvkobjs,lifetime,lifetime2,dummy);

	rdscene = mui.createmainmenuframe(mvkobjs,mnobjs);
	mui.render(mvkobjs, mvkobjs.rdcommandbuffer[0]);




	vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[0]);


	mbackground->uploadubossbo(mvkobjs, mpersviewmats);

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



	return rdscene;
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
	colorclearvalue.color = { {0.0048f,0.0048f,0.0048f,1.0f } };

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


	//if (vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence) != VK_SUCCESS) {
	//	return false;
	//}
mvkobjs.mtx2->lock();
	if (vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence) != VK_SUCCESS) {
		return false;
	}
mvkobjs.mtx2->unlock();

	//if (vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
	//	return false;
	//}

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
	colorclearvalue.color = { {0.012f,0.012f,0.012f,1.0f } };

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


void vkrenderer::drawshop() {


	animateshop();

	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		mchoices[i]->updatemats();
	}
	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		mstaticchoices[i]->updatemats();
	}



	vkWaitForFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence, VK_TRUE, UINT64_MAX);
	vkResetFences(mvkobjs.rdvkbdevice.device, 1, &mvkobjs.rdrenderfence);

	uint32_t imgidx = 0;
	VkResult res = vkAcquireNextImageKHR(mvkobjs.rdvkbdevice.device, mvkobjs.rdvkbswapchain.swapchain, UINT64_MAX, mvkobjs.rdpresentsemaphore, VK_NULL_HANDLE, &imgidx);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateswapchain();
	}



	VkClearValue colorclearvalue;
	colorclearvalue.color = { {0.12f,0.12f,0.12f,1.0f } };

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










	vkResetCommandBuffer(mvkobjs.rdcommandbuffer[0], 0);

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(mvkobjs.rdcommandbuffer[0], &cmdbgninfo);



	vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer[0], &rpinfo, VK_SUBPASS_CONTENTS_INLINE);



	vkCmdSetViewport(mvkobjs.rdcommandbuffer[0], 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.rdcommandbuffer[0], 0, 1, &scissor);

	mground->draw(mvkobjs);





	lifetime = glfwGetTime();
	lifetime2 = glfwGetTime();


	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		mchoices[i]->draw(mvkobjs, lifetime, lifetime2, shopitemcount);
	}

	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		mstaticchoices[i]->draw(mvkobjs);//TODO
	}

	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		for (const auto& j : mchoices[i]->getallinstances()) {
			j->checkforupdates();
		}
	}
	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		for (const auto& j : mstaticchoices[i]->getallinstances()) {
			j->checkforupdates();
		}
	}





	vkCmdEndRenderPass(mvkobjs.rdcommandbuffer[0]);

	for (size_t i{ 0 }; i < mchoices.size(); i++) {
		mchoices[i]->uploadubossbo(mvkobjs,mpersviewmats,enemyhps);
	}
	for (size_t i{ 0 }; i < mstaticchoices.size(); i++) {
		mstaticchoices[i]->uploadubossbo(mvkobjs, mpersviewmats);
	}


	mground->uploadubossbo(mvkobjs, mpersviewmats);




	vkEndCommandBuffer(mvkobjs.rdcommandbuffer[0]);



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
	vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence);
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
		recreateswapchain();
	}
}



