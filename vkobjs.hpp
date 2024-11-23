#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vk/VkBootstrap.h>
#include <vk/vk_mem_alloc.h>

struct vkvert {
	glm::vec3 pos;
	glm::vec3 col;
	glm::vec2 uv;
};
struct vkmesh {
	std::vector<vkvert> verts;
};
enum class skinningmode {
	linear=0,
	dualquat
};
enum class replaydirection {
	forward=0,
	backward
};
enum class blendmode {
	fadeinout=0,
	crossfade,
	additive
};
enum class ikmode {
	off=0,
	ccd,
	fabrik
};

struct vktexdata {
	VkImage teximg = VK_NULL_HANDLE;
	VkImageView teximgview = VK_NULL_HANDLE;
	VkSampler texsampler = VK_NULL_HANDLE;
	VmaAllocation teximgalloc = nullptr;

	VkDescriptorPool texdescriptorpool = VK_NULL_HANDLE;
	VkDescriptorSetLayout texdescriptorlayout = VK_NULL_HANDLE;
	VkDescriptorSet texdescriptorset = VK_NULL_HANDLE;
};

struct vkvertexbufferdata {

	size_t rdvertexbuffersize{ 0 };
	VkBuffer rdvertexbuffer = VK_NULL_HANDLE;
	VmaAllocation rdvertexbufferalloc = nullptr;
	VkBuffer rdstagingbuffer = VK_NULL_HANDLE;
	VmaAllocation rdstagingbufferalloc = nullptr;
};

struct vkindexbufferdata {
	size_t rdindexbuffersize{ 0 };
	VkBuffer rdindexbuffer = VK_NULL_HANDLE;
	VmaAllocation rdindexbufferalloc = nullptr;
	VkBuffer rdstagingbuffer = VK_NULL_HANDLE;
	VmaAllocation rdstagingbufferalloc = nullptr;
};

struct vkuniformbufferdata {
	size_t rduniformbuffersize{ 0 };
	VkBuffer rdubobuffer = VK_NULL_HANDLE;
	VmaAllocation rdubobufferalloc = nullptr;

	VkDescriptorPool rdubodescriptorpool = VK_NULL_HANDLE;
	VkDescriptorSetLayout rdubodescriptorlayout = VK_NULL_HANDLE;
	VkDescriptorSet rdubodescriptorset = VK_NULL_HANDLE;
};

struct vkshaderstoragebufferdata {
	size_t rdssbobuffersize{ 0 };
	VkBuffer rdssbobuffer = VK_NULL_HANDLE;
	VmaAllocation rdssbobufferalloc = nullptr;

	VkDescriptorPool rdssbodescriptorpool = VK_NULL_HANDLE;
	VkDescriptorSetLayout rdssbodescriptorlayout = VK_NULL_HANDLE;
	VkDescriptorSet rdssbodescriptorset = VK_NULL_HANDLE;
};

struct vkpushconstants {
	int pkmodelstride;
	unsigned int texidx;
};


struct vkobjs {

	GLFWwindow* rdwind = nullptr;
	GLFWmonitor* rdmonitor = nullptr;
	const GLFWvidmode* rdmode;
	bool rdfullscreen{ true };
	int rdwidth = 0;
	int rdheight = 0;
	unsigned int rdtricount = 0;
	unsigned int rdgltftricount = 0;
	int rdfov = 90;
	bool rdswitchshader{ false };
	float rdframetime{ 0.0f };
	float rdmatrixgeneratetime{ 0.0f };
	float rdiktime{ 0.0f };
	float rduploadtovbotime{ 0.0f };
	float rduploadtoubotime{ 0.0f };
	float rduigeneratetime{ 0.0f };
	float rduidrawtime{ 0.0f };





	// CAM

	int rdcamforward{ 0 };
	int rdcamright{ 0 };
	int rdcamup{ 0 };

	float rdtickdiff{ 0.0f };

	float rdazimuth{ 15.0f };
	float rdelevation{ -25.0f };
	glm::vec3 rdcamwpos{ 350.0f,350.0f,1000.0f };


	int rdnumberofinstances{ 0 };
	int rdcurrentselectedinstance{ 0 };






	VmaAllocator rdallocator=nullptr;

	vkb::Instance rdvkbinstance{};
	vkb::PhysicalDevice rdvkbphysdev{};
	vkb::Device rdvkbdevice{};
	vkb::Swapchain rdvkbswapchain{};

	std::vector<VkImage> rdswapchainimages;
	std::vector<VkImageView> rdswapchainimageviews;
	std::vector<VkFramebuffer> rdframebuffers;

	VkQueue rdgraphicsqueue = VK_NULL_HANDLE;
	VkQueue rdpresentqueue = VK_NULL_HANDLE;

	VkImage rddepthimage = VK_NULL_HANDLE;
	VkImageView rddepthimageview = VK_NULL_HANDLE;
	VkFormat rddepthformat;
	VmaAllocation rddepthimagealloc = VK_NULL_HANDLE;

	VkRenderPass rdrenderpass;






	VkCommandPool rdcommandpool0 = VK_NULL_HANDLE;
	VkCommandPool rdcommandpool = VK_NULL_HANDLE;
	VkCommandBuffer rdcommandbuffer0 = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> rdcommandbuffer = { VK_NULL_HANDLE,VK_NULL_HANDLE };

	VkSemaphore rdpresentsemaphore = VK_NULL_HANDLE;
	VkSemaphore rdrendersemaphore = VK_NULL_HANDLE;
	VkFence rdrenderfence = VK_NULL_HANDLE;






	VkDescriptorPool rdimguidescriptorpool = VK_NULL_HANDLE;
};

struct vkgltfobjs {
	std::vector<std::vector<std::vector<vkvertexbufferdata>>> rdgltfvertexbufferdata{};
	std::vector<std::vector<vkindexbufferdata>> rdgltfindexbufferdata{};
	std::vector<vktexdata> rdgltfmodeltex{};
};
