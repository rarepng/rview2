#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vk/VkBootstrap.h>
#include <vk/vk_mem_alloc.h>
#include <memory>
#include <thread>
#include <future>
#include <shared_mutex>
#include "netclient.hpp"
#include "netserver.hpp"

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

struct spell {
	const unsigned int spellid;
	bool active;
	bool ready;
	bool cast;
	unsigned int cooldownticks;
	unsigned int activeticks;
	unsigned int cdelapsed;
	unsigned int activeelapsed;
	unsigned int size;
	glm::vec3 pos;
	double dmg;
};

struct playerobjs {
	std::vector<spell> spells;
};

struct vktexdata {
	VkImage teximg = VK_NULL_HANDLE;
	VkImageView teximgview = VK_NULL_HANDLE;
	VkSampler texsampler = VK_NULL_HANDLE;
	VmaAllocation teximgalloc = nullptr;

};
struct vktexdatapls {
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
	float t{ 0.0f };
	bool decaying{ false };
};


enum struct pausestate {
	resumed,
	paused
};


enum struct gamestate0 {
	normal,
	dead,
	won,
	menu
	
};
enum struct gamestage {
	combat,
	shop
};
enum struct wavetype {
	horde,
	boss,
	pvp,
	event0
};

struct gobjs{
	unsigned int wave{ 0 };
};


struct netobjs {
	bool offlineplay{ true };
	bool rdserverclient{ false };
	netclient* nclient = nullptr;
	netserver* nserver = nullptr;
	std::string serveraddress{ "127.0.0.1:21122" };
	int port{21122};
};

struct vkobjs {

	inline static const std::shared_ptr<std::shared_mutex>  mtx2{ std::make_shared<std::shared_mutex>() };


	GLFWwindow* rdwind = nullptr;
	GLFWmonitor* rdmonitor = nullptr;
	const GLFWvidmode* rdmode;
	bool rdfullscreen{ false };
	int rdwidth = 0;
	int rdheight = 0;
	unsigned int rdtricount = 0;
	unsigned int rdgltftricount = 0;
	float rdfov = 1.0472f;
	bool rdswitchshader{ false };
	float rdframetime{ 0.0f };
	float rdmatrixgeneratetime{ 0.0f };
	float rdiktime{ 0.0f };
	float rduploadtovbotime{ 0.0f };
	float rduploadtoubotime{ 0.0f };
	float rduigeneratetime{ 0.0f };
	float rduidrawtime{ 0.0f };

	bool* decaying;

	float loadingprog{ 0.0f };

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



	glm::vec3 raymarchpos{ 0.0f };


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

	VkRenderPass rdrenderpass= VK_NULL_HANDLE;






	std::vector<VkCommandPool> rdcommandpool = { VK_NULL_HANDLE,VK_NULL_HANDLE };
	std::vector<VkCommandBuffer> rdcommandbuffer = { VK_NULL_HANDLE,VK_NULL_HANDLE };

	VkSemaphore rdpresentsemaphore = VK_NULL_HANDLE;
	VkSemaphore rdrendersemaphore = VK_NULL_HANDLE;
	VkFence rdrenderfence = VK_NULL_HANDLE;






	VkDescriptorPool rdimguidescriptorpool = VK_NULL_HANDLE;

};

struct vkgltfobjs {
	std::vector<std::vector<std::vector<vkvertexbufferdata>>> rdgltfvertexbufferdata{};
	std::vector<std::vector<vkindexbufferdata>> rdgltfindexbufferdata{};
	std::vector<vktexdata> tex{};
	vktexdatapls texpls{};
};
