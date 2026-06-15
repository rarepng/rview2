#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <SDL3/SDL.h>
#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <shared_mutex>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <span>
#include <iostream>
#include <deque>
#include <functional>
#include <atomic>
#include <queue>
#include <rend/graph.hpp>
#include <tracy/Tracy.hpp>
#include <fstream>
#include <map>
// all this &$#! for a stupid ugly diagram
// #ifndef __cpp_lib_move_only_function
// #ifdef __clang__
// #pragma clang diagnostic push
// #pragma clang diagnostic ignored "-Wcert-dcl58-cpp"
// #endif
// namespace std {
//     template<typename Signature>
//     struct move_only_function;

//     template<typename Ret, typename... Args>
//     struct move_only_function<Ret(Args...)> {
//         move_only_function() = default;
//         template<typename F> move_only_function(F&&) {}
//         Ret operator()(Args...) const { return Ret(); }
//     };
// }
// #ifdef __clang__
// #pragma clang diagnostic pop
// #endif
// #endif
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
		VkBufferCreateInfo bci = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
		VmaAllocationCreateInfo aci{};
		aci.usage = VMA_MEMORY_USAGE_AUTO;
		aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		            VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VmaAllocationInfo info;
		auto x = vmaCreateBuffer(allocator, &bci, &aci, &buffer, &allocation, &info);
		mappedData = static_cast<uint8_t*>(info.pMappedData);
		currentOffset = 0;

		if (x == VK_SUCCESS)return true;
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

struct alignas(16) MaterialData {
	uint32_t albedoIdx;
	uint32_t normalIdx;
	uint32_t ormIdx;
	uint32_t emissiveIdx;

	uint32_t transmissionIdx;
	uint32_t sheenIdx;
	uint32_t clearcoatIdx;
	uint32_t thicknessIdx;

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
	float envMapMaxLod;
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
	uint32_t bindless_idx = 0;
};

struct cam {
	glm::vec3 mforward{0.0f};
	glm::vec3 mright{0.0f};
	glm::vec3 mup{0.0f};
	glm::vec3 wup{0.0f, 1.0f, 0.0f};
};

struct alignas(16) GPUInstanceData {
	glm::mat4 worldTransform;
	uint32_t modelID;
	uint32_t jointOffset;
	uint32_t isSkinned;
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t  vertexOffset;
	uint32_t morphWeightOffset;
};

struct ssbodata {
	size_t size{0};
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = nullptr;
	void* mapped_data = nullptr;
};
struct vkpushconstants {
	int stride;
	float t;
	uint32_t materialID;
	uint32_t modelID;
	uint32_t frameIndex;

	uint32_t posIdx;
	uint32_t normIdx;
	uint32_t uvIdx;
	uint32_t jointIdx;
	uint32_t weightIdx;
	// coloridx
	uint32_t jointFmt;
	uint32_t weightFmt;
};

struct GlobalMaterialHeap {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = VK_NULL_HANDLE;
	VkDescriptorSet dset = VK_NULL_HANDLE;
	VkDescriptorPool dedicated_pool = VK_NULL_HANDLE;

	std::mutex mtx;
	std::queue<uint32_t> free_slots;
	void* mapped_data = nullptr;
};
struct GlobalBufferHeap {
	std::mutex mtx;
	std::queue<uint32_t> free_slots;
};


struct alignas(16) DualQuatScale {
	glm::quat real{};
	glm::quat dual{};
	glm::vec4 scale{};
};
struct alignas(16) LocalTRS {
	glm::quat R;
	glm::vec3 T;
	float pad0;
	glm::vec3 S;
	float pad1;
};

// starting segmentation

struct alignas(64) rdev {
	vkb::Instance inst{};
	vkb::Device vkdevice{};
	VmaAllocator alloc{nullptr};

	VkQueue graphicsQ{VK_NULL_HANDLE};
	VkQueue presentQ{VK_NULL_HANDLE};
	VkQueue computeQ{VK_NULL_HANDLE};
	VkQueue transferQ{VK_NULL_HANDLE};

	uint32_t graphicsQidx{};
	uint32_t presentQidx{};
	uint32_t computeQidx{};
	uint32_t transferQidx{};

	rctx ctx{};
	DeletionQueue deletionQ{};
	DeletionQueue cleanupQ{};
	StagingBelt sbelt{};

	std::array<VkDescriptorPool, 1> dpools = {VK_NULL_HANDLE};
	std::array<VkSampler, 2> samplerz{};
};

struct alignas(64) rwind {
	SDL_Window* wind = nullptr;
	const SDL_DisplayMode* rdmode = nullptr;
	SDL_Event e{};
	VkSurfaceKHR surface{};
	vkb::Swapchain schain{};
	std::vector<VkImage> schainimgs;
	std::vector<VkImageView> schainimgviews;

	int width{0};
	int height{0};
	bool fullscreen{false};
	bool mshutdown{false};

	VkImage rddepthimage = VK_NULL_HANDLE;
	VkImageView rddepthimageview = VK_NULL_HANDLE;
	VmaAllocation rddepthimagealloc = VK_NULL_HANDLE;
	VkFormat rddepthformat{};

	VkImage rddepthimageref = VK_NULL_HANDLE;
	VkImageView rddepthimageviewref = VK_NULL_HANDLE;
	VmaAllocation rddepthimageallocref = VK_NULL_HANDLE;
	VkFormat rddepthformatref{};

	struct obs_rendertarget {
		VkImage img = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		void* shared_handle = nullptr;
		uint32_t width = 1920;
		uint32_t height = 1080;
	} obs_target;
};

namespace rview {
namespace core {
inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT{3}; //fix!! different devices might not serve 3 swapchain images
inline constexpr uint32_t MAX_BINDLESS_BUFFERS = 1024;
static constexpr size_t MAX_GLOBAL_MATERIALS = 10000;
inline constexpr size_t idximguipool{0};
inline uint32_t currentFrame{0};
inline uint32_t hdrmiplod{0};
inline std::atomic<uint32_t> globalBufferCounter{1};
inline GlobalBufferHeap global_buffers{};
inline std::array<GpuBuffer, rview::core::MAX_FRAMES_IN_FLIGHT> globalInstanceBuffers{};
inline std::array<GpuBuffer, rview::core::MAX_FRAMES_IN_FLIGHT> globalIndirectBuffers{};
inline std::array<RenderGraph, rview::core::MAX_FRAMES_IN_FLIGHT> frameGraphs{};
inline GpuBuffer g_rawIndexSSBO{};
inline uint32_t g_rawIndexCapacity = 0;
inline uint32_t g_rawIndexOffset = 0;
inline GpuBuffer g_primitiveRegistrySSBO{};
inline uint32_t g_primCapacity = 0;
inline uint32_t g_primOffset = 0;
inline GpuBuffer g_modelRegistrySSBO{};
inline uint32_t g_modelCapacity = 0;
inline uint32_t g_modelOffset = 0;
inline std::array<GpuBuffer, rview::core::MAX_FRAMES_IN_FLIGHT> g_indirectCommandBuffers;
inline std::array<GpuBuffer, rview::core::MAX_FRAMES_IN_FLIGHT> g_indirectCountBuffers;
inline ssbodata globalCameraUBO{};
inline GlobalMaterialHeap global_materials{};
inline std::vector<GPUInstanceData> frameInstances;
inline VkPipelineLayout globalPipelineLayout = VK_NULL_HANDLE;
inline VkDescriptorSetLayout globalBindlessLayout = VK_NULL_HANDLE;
inline VkDescriptorPool globalBindlessPool = VK_NULL_HANDLE;
inline VkDescriptorSet globalBindlessSet = VK_NULL_HANDLE;
inline VkPipeline globalcullpline = VK_NULL_HANDLE;
inline std::atomic<uint32_t> globalTextureCounter{4};
inline std::array<texdata, 1> exrtex{};
inline const std::shared_ptr<std::shared_mutex> mtx2{std::make_shared<std::shared_mutex>()};
struct DummyTexture {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
};
inline struct {
	DummyTexture purple;
	DummyTexture white;
	DummyTexture normal;
	DummyTexture black;
} defaults;



inline glm::vec3 camwpos{0.0f, 1.6f, 2.9f};
inline cam mvkcam{};
inline float fov = 1.0472f;
inline float azimuth{0.0f};
inline float elevation{0.0f};
inline int camfor{0};
inline int camright{0};
inline int camup{0};
inline double tickdiff{0.0f};
inline float frametime{0.0f};
inline float updateanimtime{0.0f};
inline float updatemattime{0.0f};
inline float uploadubossbotime{0.0f};
inline float iksolvetime{0.0f};
inline float rduigeneratetime{0.0f};
inline float rduidrawtime{0.0f};
inline float loadingprog{0.0f};

namespace platform {
#ifdef _WIN32
constexpr bool is_windows = true;
#else
constexpr bool is_windows = false;
#endif
// TODO inject with cmake
#ifdef HEADLESS
constexpr bool headless = true;
#else
constexpr bool headless = false;
#endif
// constexpr const char* ext_memory_win32 = "VK_KHR_external_memory_win32";
};
};
namespace anim {
inline constexpr float samplerate{6.0f};
static constexpr uint32_t MAX_IK_CHAINS = 2;
};
namespace io {

};

};

struct DODAnimationClip {
	std::string name;
	float duration = 0.0f;
	float sampleRate = rview::anim::samplerate;
	uint32_t nodeCount = 0;
	uint32_t jointCount = 0;
	uint32_t frameCount = 0;

	std::vector<glm::mat4> globalTransforms;

	std::vector<glm::mat4> finalJointMatrices;
	std::vector<DualQuatScale> finalJointDQs;

	std::vector<LocalTRS> localTracks;

	inline const DualQuatScale* GetFinalJointsFrame(float time, bool loop) const {
		if (frameCount == 0 || jointCount == 0) return nullptr;

		float adjustedTime = time;

		if (loop) {
			adjustedTime = std::fmod(time, duration);
		} else {
			adjustedTime = std::min(time, duration);
		}

		uint32_t frameIndex = static_cast<uint32_t>(adjustedTime * sampleRate);

		if (frameIndex >= frameCount) frameIndex = frameCount - 1;

		return &finalJointDQs[frameIndex * jointCount];
		// return &finalJointMatrices[frameIndex * jointCount];
	}
	inline const LocalTRS* GetLocalTracksFrame(float time, bool loop) const {
		if (frameCount == 0 || nodeCount == 0) return nullptr;

		float adjustedTime = time;

		if (loop) adjustedTime = std::fmod(time, duration);
		else      adjustedTime = std::min(time, duration);

		uint32_t frameIndex = static_cast<uint32_t>(adjustedTime * sampleRate);

		if (frameIndex >= frameCount) frameIndex = frameCount - 1;

		return &localTracks[frameIndex * nodeCount];
	}
};


struct alignas(64) rframe {
	std::array<VkCommandPool, 3> cpools_graphics = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::array<VkCommandPool, 3> cpools_compute = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::array<VkCommandPool, 3> cpools_transfer = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
	std::array<VkCommandPool, 3> cpools_present = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

	std::array<std::array<VkCommandBuffer, 3>, 3> cbuffers_graphics = {{{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}}};
	std::array<std::array<VkCommandBuffer, 3>, 3> cbuffers_compute = {{{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}}};
	std::array<std::array<VkCommandBuffer, 3>, 3> cbuffers_transfer = {{{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}, {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE}}};

	std::array<std::array<VkSemaphore, 8>, 3> semaphorez {{
			{{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE }},
			{{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE }},
			{{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE }}
		}};
	std::array<VkFence, 3> fencez{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

	std::array<DeletionQueue, 3> framedeletionQ{};
};
// idk
// static_assert(sizeof(vkpushconstants) == 128, "Struct size mismatch!");
struct alignas(64) rvkbucket : public rdev, public rwind, public rframe {
	// no more bucket

};

struct vkgltfobjs {
	std::vector<std::vector<std::vector<GpuBuffer>>> vbos{};
	std::vector<std::vector<GpuBuffer>> ebos{};
	std::vector<texdata> texs{};
};
namespace rpool {
static inline bool create(const std::span<VkDescriptorPoolSize>& pools, const VkDevice& dev, VkDescriptorPool* dpool) {
	VkDescriptorPoolCreateInfo descriptorPool{};
	descriptorPool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPool.poolSizeCount = pools.size();
	descriptorPool.pPoolSizes = pools.data();
	descriptorPool.maxSets = 1024; //not sure

	return vkCreateDescriptorPool(dev, &descriptorPool, nullptr, dpool) == VK_SUCCESS;
}
static inline void destroy(const VkDevice& dev, VkDescriptorPool dpool) {
	vkDestroyDescriptorPool(dev, dpool, nullptr);
}
};
inline void safe_cleanup(rvkbucket& objs, GpuBuffer& bufferData) {
	VkBuffer buf = bufferData.buffer;
	VmaAllocation alloc = bufferData.alloc;

	if (buf != VK_NULL_HANDLE) {
		objs.deletionQ.push_function([ =, &objs]() {
			vmaDestroyBuffer(objs.alloc, buf, alloc);
		});
	}

	bufferData.buffer = VK_NULL_HANDLE;
	bufferData.alloc = nullptr;
}


inline GpuBuffer g_morphDeltaSSBO{};
inline uint32_t g_morphDeltaCapacity = 0;
inline uint32_t g_morphDeltaOffset = 0;
inline std::array<GpuBuffer, rview::core::MAX_FRAMES_IN_FLIGHT> g_morphWeightBuffers{};
