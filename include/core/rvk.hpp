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
#include <iostream>
#include <deque>
#include <functional>
#include <execution>


struct rctx {

	//function pointer (hopefully)
	PFN_vkCmdDrawMeshTasksEXT cmdDrawMeshTasks;
};

// future proof: gotta make everything use this now big TODO
struct DeletionQueue {
	std::vector<std::move_only_function<void()>> deletors;

	void push_function(std::move_only_function<void()>&& function) {
		deletors.push_back(std::move(function));
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
			(*it)();
		}
		deletors.clear();
	}
};
enum class skinningmode { linear = 0, dualquat };
enum class replaydirection { forward = 0, backward };
enum class blendmode { fadeinout = 0, crossfade, additive };
enum class ikmode { off = 0, ccd, fabrik };

struct StagingBelt {
	VkBuffer buffer;
	VmaAllocation allocation;
	uint8_t* mappedData = nullptr;
	VkDeviceSize capacity = 0;
	VkDeviceSize currentOffset = 0;

	bool init(VmaAllocator allocator, VkDeviceSize size = 256 * 1024 * 1024) {
		capacity = size;
		VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bci.size = size;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		VmaAllocationCreateInfo aci{};
		aci.usage = VMA_MEMORY_USAGE_AUTO;
		aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		            VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VmaAllocationInfo info;
		auto x = vmaCreateBuffer(allocator, &bci, &aci, &buffer, &allocation, &info);
		mappedData = static_cast<uint8_t*>(info.pMappedData);
		currentOffset = 0;
		if(x == VK_SUCCESS)return true;
		else return false;
	}

	//i have to think about syncing with this if i have 2+ staging buffers so each frame in flight has its own
	void reset() {
		currentOffset = 0;
	}

	// rethink this
	VkDeviceSize reserve(VkDeviceSize size) {
		VkDeviceSize alignedOffset = (currentOffset + 15) & ~15;
		if (alignedOffset + size > capacity) {
			std::cout << "OOM Staging Buffer!\n";
			return 0;
		}
		currentOffset = alignedOffset + size;
		return alignedOffset;
	}
	void free(VmaAllocator allocator) {
		vmaDestroyBuffer(allocator, this->buffer, this->allocation);
	}
};

struct texdata {
	VkImage img = VK_NULL_HANDLE;
	VkImageView imgview = VK_NULL_HANDLE;
	VkSampler imgsampler = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;
	uint32_t bindless_idx = 0;
};

struct GpuBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;
	VkDeviceAddress address = 0;
	VkDeviceSize size = 0;
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
	int stride{};
	float t{};

	uint32_t albedoIdx{};
	uint32_t normalIdx{};
	uint32_t ormIdx{};
	uint32_t emissiveIdx{};
	uint32_t transmissionIdx{};
	uint32_t sheenIdx{};
	uint32_t clearcoatIdx{};
	uint32_t thicknessIdx{};

	//fu padding [-2 days]
	float _pad0[2];

	glm::vec4 baseColorFactor;
	glm::vec3 emissiveFactor;
	float normalScale;

	float roughnessFactor;
	float metallicFactor;
	float transmissionFactor;
	float ior;

	glm::vec3 sheenColorFactor;
	float sheenRoughnessFactor;

	float clearcoatFactor;
	float clearcoatRoughnessFactor;
	float thicknessFactor;

	//more padding
	float envMapMaxLod;
};
// idk
// static_assert(sizeof(vkpushconstants) == 128, "Struct size mismatch!");
struct alignas(64) rvkbucket {

	vkb::Instance inst{};
	vkb::Device vkdevice{};
	VmaAllocator alloc = nullptr;
	vkb::Swapchain schain{};
	VkSurfaceKHR surface{};

	VkQueue graphicsQ = VK_NULL_HANDLE;
	VkQueue presentQ = VK_NULL_HANDLE;
	VkQueue computeQ = VK_NULL_HANDLE;
	VkQueue transferQ = VK_NULL_HANDLE;

	// tex=1
	std::array<VkCommandPool,3> cpools_graphics = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::array<VkCommandPool,3> cpools_compute = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::array<VkCommandPool,3> cpools_transfer = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	// useless apparently
	std::array<VkCommandPool,3> cpools_present = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

	std::array<std::array<VkCommandBuffer,3>,3> cbuffers_graphics = {{{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE},{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE},{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}}};
	std::array<std::array<VkCommandBuffer,3>,3> cbuffers_compute = {{{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE},{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE},{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}}};
	std::array<std::array<VkCommandBuffer,3>,3> cbuffers_transfer = {{{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE},{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE},{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}}};

	// {image available, renderfinished, compute finished}
	std::array<std::array<VkSemaphore, 3>, 3> semaphorez {{
			{{ VK_NULL_HANDLE,VK_NULL_HANDLE, VK_NULL_HANDLE }},
			{{ VK_NULL_HANDLE,VK_NULL_HANDLE, VK_NULL_HANDLE }},
			{{ VK_NULL_HANDLE,VK_NULL_HANDLE, VK_NULL_HANDLE }}
		}};
	std::array<VkFence,3> fencez{VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
	std::array<VkDescriptorPool,6> dpools = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE};
	
	
	VkImage rddepthimage = VK_NULL_HANDLE;
	VkImageView rddepthimageview = VK_NULL_HANDLE;
	VmaAllocation rddepthimagealloc = VK_NULL_HANDLE;
	VkImage rddepthimageref = VK_NULL_HANDLE;
	VkImageView rddepthimageviewref = VK_NULL_HANDLE;
	VmaAllocation rddepthimageallocref = VK_NULL_HANDLE;
	
	SDL_Window *wind = nullptr;
	const SDL_DisplayMode *rdmode;
	SDL_Event e{SDL_Event{}};

    glm::vec3 camwpos{0.0f, 6.0f, 12.0f};
    float cam_pad0; // Explicit padding to keep 16-byte alignment if needed
    glm::vec3 raymarchpos{0.0f};
    float cam_pad1; 




	rctx ctx{};
	DeletionQueue deletionQ{};
	DeletionQueue cleanupQ{};
	StagingBelt sbelt{};
	std::array<DeletionQueue,3> framedeletionQ{};

	

	std::array<texdata,1> exrtex{};
	VkDescriptorSet exrdset = VK_NULL_HANDLE;
	std::array<VkSampler,4> samplerz{};

	struct DummyTexture {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
	};

	struct {
		DummyTexture purple;
		DummyTexture white;
		DummyTexture normal;
		DummyTexture black;
	} defaults;



	double tickdiff{0.0f};
	float frametime{0.0f};
	float updateanimtime{0.0f};
	float updatemattime{0.0f};
	float uploadubossbotime{0.0f};
	float iksolvetime{0.0f};
	float rduigeneratetime{0.0f};
	float rduidrawtime{0.0f};
	float loadingprog{0.0f};



	float fov = 1.0472f;
	float azimuth{15.0f};
	float elevation{-25.0f};


	int width{0};
	int height{0};
	unsigned int tricount{0};
	unsigned int gltftricount{0};



	int camfor{0};
	int camright{0};
	int camup{0};


	uint32_t graphicsQidx{};
	uint32_t presentQidx{};
	uint32_t computeQidx{};
	uint32_t transferQidx{};


	VkFormat rddepthformat{};
	VkFormat rddepthformatref{};

	inline static const std::shared_ptr<std::shared_mutex> mtx2{std::make_shared<std::shared_mutex>()};


	inline static uint32_t MAX_FRAMES_IN_FLIGHT{3}; //fix!! different devices might not serve 3 swapchain images
	inline static uint32_t currentFrame{0};
	inline static uint32_t hdrmiplod{0};


	inline static std::shared_ptr<VkDescriptorSetLayout> texlayout = std::make_shared<VkDescriptorSetLayout>();
	inline static VkDescriptorSetLayout ubolayout = VK_NULL_HANDLE;
	inline static VkDescriptorSetLayout ssbolayout = VK_NULL_HANDLE;
	inline static VkDescriptorSetLayout hdrlayout = VK_NULL_HANDLE;
	inline static constexpr size_t idxinitpool{0};
	inline static constexpr size_t idximguipool{1};
	inline static constexpr size_t idxruntimepool0{2};
	inline static constexpr size_t idxruntimepool1{3};
	inline static constexpr size_t idxruntimepool2{4};
	inline static constexpr size_t idxruntimepool3{5};

	bool fullscreen{false};
	bool mshutdown{false};


	std::vector<VkImage> schainimgs;
	std::vector<VkImageView> schainimgviews;


};
struct vkgltfobjs {
	std::vector<std::vector<std::vector<GpuBuffer>>> vbos{};
	std::vector<std::vector<GpuBuffer>> ebos{};
	std::vector<texdata> texs{};
	VkDescriptorSet dset = VK_NULL_HANDLE;
	VkDescriptorPool texpool = VK_NULL_HANDLE;
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
inline void safe_cleanup(rvkbucket& objs, GpuBuffer& bufferData) {
	VkBuffer buf = bufferData.buffer;
	VmaAllocation alloc = bufferData.alloc;
	if (buf != VK_NULL_HANDLE) {
		objs.deletionQ.push_function([=, &objs]() {
			vmaDestroyBuffer(objs.alloc, buf, alloc);
		});
	}
	bufferData.buffer = VK_NULL_HANDLE;
	bufferData.alloc = nullptr;
}