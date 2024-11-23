#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION
#include <imgui/imgui_impl_glfw.h>
#include <glm/gtc/matrix_transform.hpp>
#include <ctime>
#include <cstdlib>
#include <glm/gtx/spline.hpp>
#include <vk/vk_mem_alloc.h>
#include "vkrenderer.hpp"
#include <iostream>
vkrenderer::vkrenderer(GLFWwindow* wind) {
	mvkobjs.rdwind = wind;
	mpersviewmats.emplace_back(glm::mat4{ 1.0f });
	mpersviewmats.emplace_back(glm::mat4{ 1.0f });
}
bool vkrenderer::init() {
	std::srand(static_cast<int>(time(NULL)));
	mvkobjs.rdheight = mvkobjs.rdvkbswapchain.extent.height;
	mvkobjs.rdwidth = mvkobjs.rdvkbswapchain.extent.width;
	if (!mvkobjs.rdwind)return false;
	if (!deviceinit())return false;
	if (!initvma())return false;
	if (!getqueue())return false;
	if (!createswapchain())return false;
	if (!createdepthbuffer())return false;
	if (!createcommandpool())return false;
	if (!createcommandbuffer())return false;
	if (!setupmodels())return false;
	if (!createrenderpass())return false;
	if (!setupmodels2())return false;
	//if (!creategltfmeshpipeline())return false;
	if (!createframebuffer())return false;
	if (!createsyncobjects())return false;
	if (!initui())return false;

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;


	mframetimer.start();


	mmodeljointmatrices.reserve(2);
	mmodeljointmatrices.resize(2);
	mmodeljointdualquats.reserve(2);
	mmodeljointdualquats.resize(2);

	return true;
}
bool vkrenderer::setupmodels() {
	mpgltf = std::make_shared<playoutmodel>();
	mpgltf2 = std::make_shared<playoutmodel>();
	if (!mpgltf->setup(mvkobjs, "untitled.glb"))return false;
	if (!mpgltf2->setup(mvkobjs, "untitled1.glb"))return false;
	return true;
}
bool vkrenderer::setupmodels2() {
	if (!mpgltf->setup2(mvkobjs, "shader/gltf_gpu.vert.spv", "shader/gltf_gpu.frag.spv"))return false;
	if (!mpgltf2->setup2(mvkobjs, "shader/gltf_gpu.vert.spv", "shader/gltf_gpu.frag.spv"))return false;
	return true;
}
bool vkrenderer::deviceinit() {
	vkb::InstanceBuilder instbuild;
	auto instret = instbuild.use_default_debug_messenger().request_validation_layers().require_api_version(1,3,0).build();
	mvkobjs.rdvkbinstance = instret.value();

	VkResult res = VK_ERROR_UNKNOWN;
	res = glfwCreateWindowSurface(mvkobjs.rdvkbinstance, mvkobjs.rdwind, nullptr, &msurface);

	vkb::PhysicalDeviceSelector physicaldevsel{ mvkobjs.rdvkbinstance };
	auto firstphysicaldevselret = physicaldevsel.set_surface(msurface).select();

	VkPhysicalDeviceMeshShaderFeaturesEXT physmeshfeatures;
	VkPhysicalDeviceVulkan13Features physfeatures13;
	//VkPhysicalDevice8BitStorageFeatures b8storagefeature;
	VkPhysicalDeviceVulkan12Features physfeatures12;
	//VkPhysicalDeviceVulkan11Features physfeatures11;
	VkPhysicalDeviceFeatures2 physfeatures;
	//b8storagefeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
	physmeshfeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	physfeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	physfeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	physfeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	physmeshfeatures.pNext = &physfeatures12;
	physfeatures.pNext = &physmeshfeatures;
	physfeatures13.pNext = VK_NULL_HANDLE;
	//b8storagefeature.pNext = VK_NULL_HANDLE;
	physfeatures12.pNext = &physfeatures13;
	vkGetPhysicalDeviceFeatures2(firstphysicaldevselret.value(), &physfeatures);
	std::cout << "\n\n\n\n" << physfeatures12.runtimeDescriptorArray << std::endl;
	if (physmeshfeatures.meshShader == VK_FALSE) {
		std::cout << "NO mesh shader support"  << std::endl;
	}
	if (physmeshfeatures.taskShader == VK_FALSE) {
		std::cout << "NO task shader support" << std::endl;
	}
	//physmeshfeatures.meshShader = VK_TRUE;
	//physmeshfeatures.taskShader = VK_TRUE;
	physmeshfeatures.multiviewMeshShader = VK_FALSE;
	physmeshfeatures.primitiveFragmentShadingRateMeshShader = VK_FALSE;

	auto secondphysicaldevselret = physicaldevsel.set_minimum_version(1,3).set_surface(msurface).set_required_features(physfeatures.features).add_required_extension_features(physmeshfeatures).set_required_features_12(physfeatures12).set_required_features_13(physfeatures13).add_required_extension("VK_EXT_mesh_shader").select();

	std::cout <<"\n\n\n\n\n\n\n\n\n\n mesh shader value: " << secondphysicaldevselret.value().is_extension_present("VK_EXT_mesh_shader")<< "\n\n\n\n\n";
	mvkobjs.rdvkbphysdev = secondphysicaldevselret.value();

	mminuniformbufferoffsetalignment = mvkobjs.rdvkbphysdev.properties.limits.minUniformBufferOffsetAlignment;

	vkb::DeviceBuilder devbuilder{ mvkobjs.rdvkbphysdev };
	auto devbuilderret = devbuilder.build();
	mvkobjs.rdvkbdevice = devbuilderret.value();

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

bool vkrenderer::createubo() {
	mvkobjs.rdperspviewmatrixubo.reserve(2);
	mvkobjs.rdperspviewmatrixubo.resize(2);

	if (!ubo::init(mvkobjs, mvkobjs.rdperspviewmatrixubo[0])) {
		return false;
	}
	if (!ubo::init(mvkobjs, mvkobjs.rdperspviewmatrixubo[1])) {
		return false;
	}
	return true;
}

bool vkrenderer::creatematssbo() {


	mvkobjs.rdjointmatrixssbo.reserve(2);
	mvkobjs.rdjointmatrixssbo.resize(2);


	size_t modelJointMatrixBufferSize = mvkobjs.rdnumberofinstances * mgltfinstances.at(0)->getjointmatrixsize() * sizeof(glm::mat4);
	size_t modelJointMatrixBufferSize2 = mvkobjs.rdnumberofinstances * mgltfinstances2.at(0)->getjointmatrixsize() * sizeof(glm::mat4);

	if (!ssbo::init(mvkobjs, mvkobjs.rdjointmatrixssbo[0], modelJointMatrixBufferSize)) {
		return false;
	}
	if (!ssbo::init(mvkobjs, mvkobjs.rdjointmatrixssbo[1], modelJointMatrixBufferSize2)) {
		return false;
	}
	return true;
}

bool vkrenderer::createdqssbo() {
	mvkobjs.rdjointdualquatssbo.reserve(2);
	mvkobjs.rdjointdualquatssbo.resize(2);

	size_t modelJointdqSize = mvkobjs.rdnumberofinstances * mgltfinstances.at(0)->getjointdualquatssize() * sizeof(glm::mat2x4);
	size_t modelJointdqSize2 = mvkobjs.rdnumberofinstances * mgltfinstances2.at(0)->getjointdualquatssize() * sizeof(glm::mat2x4);

	if (!ssbo::init(mvkobjs, mvkobjs.rdjointdualquatssbo[0], modelJointdqSize)) {
		return false;
	}
	if (!ssbo::init(mvkobjs, mvkobjs.rdjointdualquatssbo[1], modelJointdqSize2)) {
		return false;
	}
	return true;
}

bool vkrenderer::createmeshssbo()
{

	if (!ssbomesh::init(mvkobjs, mvkobjs.rdmeshssbo)) {
		return false;
	}
	return true;
}

bool vkrenderer::createrenderpass() {
	if (!renderpass::init(mvkobjs))return false;
	return true;
}


//bool vkrenderer::creategltfpipelinelayout() {
//	std::vector<vktexdata> texdata0 = mgltf->gettexdata();
//	if (!playout::init(mvkobjs, texdata0, mvkobjs.rdgltfpipelinelayout))return false;
//	return true;
//}
//bool vkrenderer::creategltfpipelinelayout2()
//{
//	std::vector<vktexdata> texdata1 = mgltf2->gettexdata();
//	if (!playout::init(mvkobjs, texdata1, mvkobjs.rdgltfpipelinelayout2))return false;
//	return true;
//}
bool vkrenderer::creategltfgpupipeline() {
	std::string vfile = "shader/gltf_gpu.vert.spv";
	std::string ffile = "shader/gltf_gpu.frag.spv";
	if (!gltfgpupipeline::init(mvkobjs, mvkobjs.rdgltfpipelinelayout, mvkobjs.rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile)) { return false; std::cout << std::endl << " failed to create gltf shaders" << std::endl; }
	if (!gltfgpupipeline::init(mvkobjs, mvkobjs.rdgltfpipelinelayout2, mvkobjs.rdgltfgpupipeline2, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile)) { return false; std::cout << std::endl << " failed to create gltf shaders" << std::endl; }
	return true;
}
bool vkrenderer::creategltfgpudqpipeline() {
	std::string vfile = "shader/gltf_gpu_dquat.vert.spv";
	std::string ffile = "shader/gltf_gpu_dquat.frag.spv";
	if (!gltfgpupipeline::init(mvkobjs, mvkobjs.rdgltfpipelinelayout, mvkobjs.rdgltfgpudqpipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile)) { return false; std::cout << std::endl << " failed to create gltf shaders" << std::endl; }
	if (!gltfgpupipeline::init(mvkobjs, mvkobjs.rdgltfpipelinelayout2, mvkobjs.rdgltfgpudqpipeline2, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vfile, ffile)) { return false; std::cout << std::endl << " failed to create gltf shaders" << std::endl; }
	return true;
}

bool vkrenderer::creategltfmeshpipeline()
{
	std::string tfile = "shader/gltfmeshgpu.task.spv";
	std::string mfile = "shader/gltfmeshgpu.mesh.spv";
	std::string ffile = "shader/gltfmeshgpu.frag.spv";
	if (!gltfmeshpipeline::init(mvkobjs, mvkobjs.rdgltfpipelinelayout, mvkobjs.rdgltfmeshpipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, tfile, mfile,ffile)) { return false; std::cout << std::endl << " failed to create gltf shaders" << std::endl; }
	return true;
}

bool vkrenderer::createframebuffer() {
	if (!framebuffer::init(mvkobjs))return false;
	return true;
}
bool vkrenderer::createcommandpool() {
	if (!commandpool::init(mvkobjs))return false;
	return true;
}
bool vkrenderer::createcommandbuffer() {
	if (!commandbuffer::init(mvkobjs, mvkobjs.rdcommandbuffer))return false;
	return true;
}
bool vkrenderer::createsyncobjects() {
	if (!syncobjects::init(mvkobjs))return false;
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

bool vkrenderer::initui() {
	if (!mui.init(mvkobjs)) {
		return false;
	}
	return true;
}

//
//bool vkrenderer::loadgltfmodel() {
//	mgltf = std::make_shared<vkgltfmodel>();
//	std::string file = "untitled1.glb";
//	if (!mgltf->loadmodel(mvkobjs, file))return false;
//	return true;
//}
//
//bool vkrenderer::loadgltfmodel2()
//{
//	mgltf2 = std::make_shared<vkgltfmodel>();
//	std::string file = "untitled.glb";
//	if (!mgltf2->loadmodel(mvkobjs, file))return false;
//	return true;
//}

//
//
//bool vkrenderer::createinstances() {
//	int numTriangles = 0;
//
//
//	//NUMBER OF INSTANCES!!
//	for (int i = 0; i < 2; ++i) {
//		int xPos = std::rand() % 99;
//		int zPos = std::rand() % 99;
//		mgltfinstances.emplace_back(std::make_shared<vkgltfinstance>(mgltf,
//			glm::vec2(static_cast<float>(xPos), static_cast<float>(zPos)), true));
//		numTriangles += mgltf->gettricount(0);
//	}
//	for (int i = 0; i < 2; ++i) {
//		int xPos = std::rand() % 99;
//		int zPos = std::rand() % 99;
//		mgltfinstances2.emplace_back(std::make_shared<vkgltfinstance>(mgltf2,
//			glm::vec2(static_cast<float>(xPos), static_cast<float>(zPos)), true));
//		numTriangles += mgltf2->gettricount(0);
//	}
//
//	mvkobjs.rdtricount = numTriangles;
//	mvkobjs.rdnumberofinstances = mgltfinstances.size();
//
//	if (!mgltfinstances.size()) {
//		return false;
//	}
//	if (!mgltfinstances2.size()) {
//		return false;
//	}
//
//	return true;
//}

void vkrenderer::cleanup() {
	vkDeviceWaitIdle(mvkobjs.rdvkbdevice.device);


	mpgltf->cleanupmodels(mvkobjs);
	mpgltf2->cleanupmodels(mvkobjs);
	mui.cleanup(mvkobjs);


	syncobjects::cleanup(mvkobjs);
	commandbuffer::cleanup(mvkobjs, mvkobjs.rdcommandbuffer);
	commandpool::cleanup(mvkobjs);
	framebuffer::cleanup(mvkobjs);


	mpgltf->cleanuplines(mvkobjs);
	mpgltf2->cleanuplines(mvkobjs);

	renderpass::cleanup(mvkobjs);

	mpgltf->cleanupbuffers(mvkobjs);
	mpgltf2->cleanupbuffers(mvkobjs);

	vkDestroyImageView(mvkobjs.rdvkbdevice.device, mvkobjs.rddepthimageview, nullptr);
	vmaDestroyImage(mvkobjs.rdallocator, mvkobjs.rddepthimage, mvkobjs.rddepthimagealloc);
	vmaDestroyAllocator(mvkobjs.rdallocator);

	mvkobjs.rdvkbswapchain.destroy_image_views(mvkobjs.rdswapchainimageviews);
	vkb::destroy_swapchain(mvkobjs.rdvkbswapchain);

	vkb::destroy_device(mvkobjs.rdvkbdevice);
	vkb::destroy_surface(mvkobjs.rdvkbinstance.instance, msurface);
	vkb::destroy_instance(mvkobjs.rdvkbinstance);


}
void vkrenderer::handlekey(int key, int scancode, int action, int mods)
{
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_SPACE) == GLFW_PRESS) {
		switchshader = !switchshader;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(mvkobjs.rdwind, true);
	}
}
int clickz{ 0 };
void vkrenderer::handleclick(int key, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (key >= 0 && key < ImGuiMouseButton_COUNT) {
		io.AddMouseButtonEvent(key, action == GLFW_PRESS);
	}
	if (io.WantCaptureMouse)return;
	

	if(key==GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	ubo::upload(mvkobjs, mvkobjs.rdperspviewmatrixubo[0], (clickz++) % 3);

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
void vkrenderer::handlemouse(double x, double y)
{




	ImGuiIO& io = ImGui::GetIO();
	io.AddMousePosEvent((float)x, (float)y);
	if (io.WantCaptureMouse) {
		return;
	}

	int relativex = static_cast<int>(x) - mousex;
	int relativey = static_cast<int>(y) - mousey;

	if (mlock) {
		mvkobjs.rdazimuth += relativex / 10.0;
		if (mvkobjs.rdazimuth < 0.0) {
			mvkobjs.rdazimuth += 360.0;
		}
		if (mvkobjs.rdazimuth >= 360.0) {
			mvkobjs.rdazimuth -= 360.0;
		}

		mvkobjs.rdelevation -= relativey / 10.0;
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
void vkrenderer::setsize(unsigned int w, unsigned int h) {
	mvkobjs.rdwidth = w;
	mvkobjs.rdheight = h;

	std::cout << "size changed";
}
void vkrenderer::toggleshader() {
	mvkobjs.rdswitchshader = !mvkobjs.rdswitchshader;
}
void vkrenderer::movecam() {

	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return;
	}

	mvkobjs.rdcamforward = 0;
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_W) == GLFW_PRESS) {
		mvkobjs.rdcamforward += 1;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_S) == GLFW_PRESS) {
		mvkobjs.rdcamforward -= 1;
	}
	mvkobjs.rdcamright = 0;
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_A) == GLFW_PRESS) {
		mvkobjs.rdcamright += 1;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_D) == GLFW_PRESS) {
		mvkobjs.rdcamright -= 1;
	}
	mvkobjs.rdcamup = 0;
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_E) == GLFW_PRESS) {
		mvkobjs.rdcamup += 1;
	}
	if (glfwGetKey(mvkobjs.rdwind, GLFW_KEY_Q) == GLFW_PRESS) {
		mvkobjs.rdcamup -= 1;
	}

	if ((glfwGetKey(mvkobjs.rdwind, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)) {
		mvkobjs.rdcamforward *= 400;
		mvkobjs.rdcamright *= 400;
		mvkobjs.rdcamup *= 400;
	}


}








bool vkrenderer::draw() {
	double tick = glfwGetTime();
	mvkobjs.rdtickdiff = tick - mlasttick;


	mvkobjs.rdframetime = mframetimer.stop();
	mframetimer.start();

	movecam();




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
	viewport.y =static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
	viewport.width = static_cast<float>(mvkobjs.rdvkbswapchain.extent.width);
	viewport.height = -static_cast<float>(mvkobjs.rdvkbswapchain.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	scissor.extent = mvkobjs.rdvkbswapchain.extent;





	mmatgentimer.start();

	mpersviewmats.at(0) = mcam.getview(mvkobjs);
	mpersviewmats.at(1) = glm::perspective(glm::radians(static_cast<float>(mvkobjs.rdfov)), static_cast<float>(mvkobjs.rdvkbswapchain.extent.width) / static_cast<float>(mvkobjs.rdvkbswapchain.extent.height), 0.01f, 6000.0f);

	//if (mgltfinstances.at(mvkobjs.rdcurrentselectedinstance)->getinstancesettings().msplayanimation) {
		mvkobjs.rdiktime = 0.0f;
	//}

		mpgltf->updateanims();
		mpgltf2->updateanims();

	//int selectedInstance = mvkobjs.rdcurrentselectedinstance;
	int selectedInstance = 0;
	//glm::vec2 modelWorldPos = mgltfinstances.at(selectedInstance)->getwpos();
	//glm::quat modelWorldRot = mgltfinstances.at(selectedInstance)->getwrot();

	mvkobjs.rdmatrixgeneratetime = mmatgentimer.stop();




	if (vkResetCommandBuffer(mvkobjs.rdcommandbuffer, 0) != VK_SUCCESS)return false;

	VkCommandBufferBeginInfo cmdbgninfo{};
	cmdbgninfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbgninfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(mvkobjs.rdcommandbuffer, &cmdbgninfo) != VK_SUCCESS)return false;

	muploadtovbotimer.start();
	mpgltf->uploadvboebo(mvkobjs);
	mpgltf2->uploadvboebo(mvkobjs);
	mvkobjs.rduploadtovbotime = muploadtovbotimer.stop();


	mpgltf->updatemats();
	mpgltf2->updatemats();


	vkCmdBeginRenderPass(mvkobjs.rdcommandbuffer, &rpinfo, VK_SUBPASS_CONTENTS_INLINE);





	vkCmdSetViewport(mvkobjs.rdcommandbuffer, 0, 1, &viewport);
	vkCmdSetScissor(mvkobjs.rdcommandbuffer, 0, 1, &scissor);




	mpgltf->draw(mvkobjs);
	mpgltf2->draw(mvkobjs);


	muigentimer.start();
	modelsettings settings = mpgltf->getinstsettings();
	mui.createframe(mvkobjs, settings);
	mvkobjs.rduigeneratetime = muigentimer.stop();
	muidrawtimer.start();
	mui.render(mvkobjs);
	mvkobjs.rduidrawtime = muidrawtimer.stop();




	vkCmdEndRenderPass(mvkobjs.rdcommandbuffer);



	muploadtoubotimer.start();


	mpgltf->uploadubossbo(mvkobjs, mpersviewmats);
	mpgltf2->uploadubossbo(mvkobjs, mpersviewmats);

	mvkobjs.rduploadtoubotime = muploadtoubotimer.stop();


	if (vkEndCommandBuffer(mvkobjs.rdcommandbuffer) != VK_SUCCESS)return false;



	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitinfo.pWaitDstStageMask = &waitstage;

	submitinfo.waitSemaphoreCount = 1;
	submitinfo.pWaitSemaphores = &mvkobjs.rdpresentsemaphore;

	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &mvkobjs.rdrendersemaphore;

	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &mvkobjs.rdcommandbuffer;


	if (vkQueueSubmit(mvkobjs.rdgraphicsqueue, 1, &submitinfo, mvkobjs.rdrenderfence) != VK_SUCCESS) {
		return false;
	}

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.waitSemaphoreCount = 1;
	presentinfo.pWaitSemaphores = &mvkobjs.rdrendersemaphore;

	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &mvkobjs.rdvkbswapchain.swapchain;

	presentinfo.pImageIndices = &imgidx;

	res = vkQueuePresentKHR(mvkobjs.rdpresentqueue, &presentinfo);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return recreateswapchain();
	}
	else {
		if (res != VK_SUCCESS) {
			return false;
		}
	}
	mlasttick = tick;


	return true;
}