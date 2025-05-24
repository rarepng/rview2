#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <SDL3/SDL.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <memory>
#include <shared_mutex>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

enum class skinningmode { linear = 0, dualquat };
enum class replaydirection { forward = 0, backward };
enum class blendmode { fadeinout = 0, crossfade, additive };
enum class ikmode { off = 0, ccd, fabrik };

struct texdata {
	VkImage img = VK_NULL_HANDLE;
	VkImageView imgview = VK_NULL_HANDLE;
	VkSampler imgsampler = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;
};
struct texdatapls {
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSetLayout dlayout = VK_NULL_HANDLE;
	VkDescriptorSet dset = VK_NULL_HANDLE;
};

struct vbodata {

	size_t size{0};
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;
	//staging
	VkBuffer sbuffer = VK_NULL_HANDLE;
	VmaAllocation salloc = nullptr;
};

struct ebodata {
	size_t size{0};
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;
	//staging
	VkBuffer sbuffer = VK_NULL_HANDLE;
	VmaAllocation salloc = nullptr;
};

struct ubodata {
	size_t size{0};
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;

	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSetLayout dlayout = VK_NULL_HANDLE;
	VkDescriptorSet dset = VK_NULL_HANDLE;
};

struct ssbodata {
	size_t size{0};
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;

	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSetLayout dlayout = VK_NULL_HANDLE;
	VkDescriptorSet dset = VK_NULL_HANDLE;
};

struct vkpushconstants {
	int stride;
	unsigned int texidx;
	float t{0.0f};
	float dmg{0.0f};
};

struct vkobjs {

	inline static const std::shared_ptr<std::shared_mutex> mtx2{std::make_shared<std::shared_mutex>()};

	SDL_Window *rdwind = nullptr;
	
	const SDL_DisplayMode *rdmode;
	bool rdfullscreen{false};
	int rdwidth{0};
	int rdheight{0};
	unsigned int rdtricount{0};
	unsigned int rdgltftricount{0};
	float rdfov = 1.0472f;
	bool rdswitchshader{false};

	SDL_Event *e;

	float frametime{0.0f};
	float updateanimtime{0.0f};
	float updatemattime{0.0f};
	float uploadubossbotime{0.0f};
	float iksolvetime{0.0f};
	float rduigeneratetime{0.0f};
	float rduidrawtime{0.0f};

	bool *mshutdown{nullptr};

	float loadingprog{0.0f};

	int rdcamforward{0};
	int rdcamright{0};
	int rdcamup{0};

	double tickdiff{0.0f};

	float rdazimuth{15.0f};
	float rdelevation{-25.0f};
	glm::vec3 rdcamwpos{350.0f, 350.0f, 1000.0f};

	glm::vec3 raymarchpos{0.0f};

	VmaAllocator rdallocator = nullptr;

	vkb::Instance rdvkbinstance{};
	vkb::PhysicalDevice rdvkbphysdev{};
	vkb::Device rdvkbdevice{};
	vkb::Swapchain rdvkbswapchain{};

	std::vector<VkImage> rdswapchainimages;
	std::vector<VkImageView> rdswapchainimageviews;
	std::vector<VkFramebuffer> rdframebuffers;
	std::vector<VkFramebuffer> rdframebufferrefs;

	VkQueue graphicsQ = VK_NULL_HANDLE;
	VkQueue presentQ = VK_NULL_HANDLE;
	VkQueue computeQ = VK_NULL_HANDLE;

	VkImage rddepthimage = VK_NULL_HANDLE;
	VkImageView rddepthimageview = VK_NULL_HANDLE;
	VkFormat rddepthformat;
	VmaAllocation rddepthimagealloc = VK_NULL_HANDLE;

	VkImage rddepthimageref = VK_NULL_HANDLE;
	VkImageView rddepthimageviewref = VK_NULL_HANDLE;
	VkFormat rddepthformatref;
	VmaAllocation rddepthimageallocref = VK_NULL_HANDLE;

	VkRenderPass rdrenderpass = VK_NULL_HANDLE;
	VkRenderPass rdrenderpass2 = VK_NULL_HANDLE;

	std::vector<VkCommandPool> rdcommandpool = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::vector<VkCommandBuffer> rdcommandbuffer = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

	VkSemaphore rdpresentsemaphore = VK_NULL_HANDLE;
	VkSemaphore rdrendersemaphore = VK_NULL_HANDLE;
	VkFence rdrenderfence = VK_NULL_HANDLE;
	VkFence rduploadfence = VK_NULL_HANDLE;

	VkDescriptorPool rdimguidescriptorpool = VK_NULL_HANDLE;
};

struct vkgltfobjs {
	std::vector<std::vector<std::vector<vbodata>>> vbodata{};
	std::vector<std::vector<ebodata>> ebodata{};
	std::vector<texdata> tex{};
	texdatapls texpls{};
};
