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
#include <span>




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

	VkDescriptorSet dset = VK_NULL_HANDLE;
};

struct ssbodata {
	size_t size{0};
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;

	VkDescriptorSet dset = VK_NULL_HANDLE;
};
struct vkpushconstants {
	int stride;
	unsigned int texidx;
	float t{0.0f};
};

struct rvk {

	inline static const std::shared_ptr<std::shared_mutex> mtx2{std::make_shared<std::shared_mutex>()};

	SDL_Window *wind = nullptr;

	const SDL_DisplayMode *rdmode;
	bool fullscreen{false};
	int width{0};
	int height{0};
	unsigned int tricount{0};
	unsigned int gltftricount{0};
	float fov = 1.0472f;

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

	int camfor{0};
	int camright{0};
	int camup{0};

	double tickdiff{0.0f};

	float azimuth{15.0f};
	float elevation{-25.0f};
	glm::vec3 camwpos{350.0f, 350.0f, 1000.0f};

	glm::vec3 raymarchpos{0.0f};

	VmaAllocator alloc = nullptr;

	vkb::Instance inst{};
	vkb::PhysicalDevice physdev{};
	vkb::Device vkdevice{};
	vkb::Swapchain schain{};

	
	
	inline static std::shared_ptr<VkDescriptorSetLayout> texlayout = std::make_shared<VkDescriptorSetLayout>();
	inline static VkDescriptorSetLayout ubolayout = VK_NULL_HANDLE;
	inline static VkDescriptorSetLayout ssbolayout = VK_NULL_HANDLE;

	std::vector<VkImage> schainimgs;
	std::vector<VkImageView> schainimgviews;
	std::vector<VkFramebuffer> fbuffers = {VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};

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

	std::array<VkCommandPool,3> cpools_graphics = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::array<VkCommandPool,1> cpools_compute = {VK_NULL_HANDLE};
	std::array<VkCommandBuffer,3> cbuffers_graphics = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::array<VkCommandBuffer,1> cbuffers_compute = {VK_NULL_HANDLE};

	VkSemaphore presentsemaphore = VK_NULL_HANDLE;
	VkSemaphore rendersemaphore = VK_NULL_HANDLE;
	VkFence renderfence = VK_NULL_HANDLE;
	VkFence uploadfence = VK_NULL_HANDLE;
	
	inline static constexpr size_t idxinitpool{0};
	inline static constexpr size_t idximguipool{1};
	inline static constexpr size_t idxruntimepool0{2};
	inline static constexpr size_t idxruntimepool1{3};
	inline static constexpr size_t idxruntimepool2{4};
	inline static constexpr size_t idxruntimepool3{5};
	std::array<VkDescriptorPool,6> dpools = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
};

struct vkgltfobjs {
	std::vector<std::vector<std::vector<vbodata>>> vbos{};
	std::vector<std::vector<ebodata>> ebos{};
	std::vector<texdata> texs{};
	VkDescriptorSet dset = VK_NULL_HANDLE;
};
namespace rpool {
static inline bool create(const std::span<VkDescriptorPoolSize>& pools,const VkDevice& dev,VkDescriptorPool* dpool) {
	VkDescriptorPoolCreateInfo descriptorPool{};
	descriptorPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPool.poolSizeCount = pools.size();
	descriptorPool.pPoolSizes = pools.data();
	descriptorPool.maxSets = 1024; //not sure

	return vkCreateDescriptorPool(dev, &descriptorPool, nullptr, dpool) == VK_SUCCESS;
}
static inline void destroy(const VkDevice& dev,VkDescriptorPool dpool) {
	vkDestroyDescriptorPool(dev, dpool, nullptr);
}
};
namespace rbuffer {
static inline bool create(const VkDevice& dev,VmaAllocator alloc,std::vector<VkBufferCreateInfo> binfos,std::vector<VkBuffer> buffs) {
	// for()
	return true;
}
};