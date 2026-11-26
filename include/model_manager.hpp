#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <core/rvk.hpp>
#include <core/scene.hpp>
#include <anim/flatskelly.hpp>
#include <anim/baker2.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <vector>
#include <string>
#include <array>
#include <mutex>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <core/scene.hpp>
#include <vktex.hpp>
#include <core/jobs.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <dbg/metrics.hpp>

enum struct AnimMode : uint8_t { Baked = 0, Hybrid = 1, Dynamic = 2, Hero = 3 };


inline DualQuatScale Mat4ToDualQuatScale(const glm::mat4& m) {
	DualQuatScale dq;

	dq.scale.x = glm::length(glm::vec3(m[0]));
	dq.scale.y = glm::length(glm::vec3(m[1]));
	dq.scale.z = glm::length(glm::vec3(m[2]));
	dq.scale.w = 0.0f;

	glm::mat3 rotMat{glm::vec3(m[0]), glm::vec3(m[1]), glm::vec3(m[2])};

	if (glm::determinant(rotMat) < 0.0f) {
		dq.scale.x *= -1.0f;
	}

	rotMat[0] /= dq.scale.x;
	rotMat[1] /= dq.scale.y;
	rotMat[2] /= dq.scale.z;

	dq.real = glm::quat_cast(rotMat);
	glm::vec3 translation = glm::vec3(m[3]);

	glm::quat tQuat(0.0f, translation.x, translation.y, translation.z);
	dq.dual = (tQuat * dq.real) * 0.5f;

	return dq;
}

namespace model_manager {
	inline std::vector<std::vector<DualQuatScale>> g_joint_final_matrices;
	inline std::array<std::atomic<int>, rview::core::MAX_FRAMES_IN_FLIGHT> g_anim_jobs_pending{};
	inline float g_anim_time_accumulator = 0.0f;
	inline std::vector<GpuBuffer> g_gpu_joint_buffers;
	inline std::atomic<int> g_pending_anim_chunks{0};
	inline std::atomic<bool> g_animations_ready{false};

	// MAGIC
	constexpr uint32_t INVALID_ANIM_POOL = 0xFFFFFFFF;
	constexpr uint32_t FALLBACK_POOL_HANDLE = 0;
	constexpr uint32_t UNIVERSAL_SAFE_JOINT_OFFSET = 0;
	constexpr uint32_t MAX_SAFE_JOINTS = 512;

	constexpr uint32_t MAX_ANIM_POOLS = 16384;

	inline std::array<tlarena, rview::core::MAX_FRAMES_IN_FLIGHT> g_anim_arenas;

	inline std::array<uint8_t, MAX_ANIM_POOLS> g_pool_used{};
	inline std::array<std::array<uint8_t, MAX_ANIM_POOLS>, rview::core::MAX_FRAMES_IN_FLIGHT> g_pool_copied{};
	inline bool g_pool_used_valid = false;
	inline uint32_t g_cpu_baked_pools = 0;

	inline uint64_t GenerateSkeletonSignature(const ParsedSkeleton& skel) {
		uint64_t hash = 2526925769548455446ull;
		auto hash_combine = [&hash](uint32_t val) {
			hash ^= val;
			hash *= 2699254256214ull;
		};

		hash_combine(skel.nodeCount);
		hash_combine(skel.jointCount);

		for (uint32_t i = 0; i < skel.nodeCount; ++i) {
			hash_combine(static_cast<uint32_t>(skel.parentIndices[i]));
		}

		for (uint32_t i = 0; i < skel.jointCount; ++i) {
			hash_combine(skel.jointToNodeMap[i]);
		}

		return hash;
	}


	enum AnimPipelineFlags : uint8_t {
		ANIM_FLAG_NONE = 0,

		ANIM_FLAG_CPU_BAKED = 1 << 0,
		ANIM_FLAG_GPU_BAKED = 1 << 1,
		ANIM_FLAG_CPU_EVAL = 1 << 2,
		ANIM_FLAG_GPU_EVAL = 1 << 3,

		ANIM_FLAG_BLENDING = 1 << 4,
		ANIM_FLAG_IK = 1 << 5
	};

	enum struct ExecutionDomain : uint8_t { CPU_BAKED = 0, GPU_BAKED = 1, CPU_EVAL = 2, GPU_EVAL = 3 };

	constexpr AnimPipelineFlags operator|(AnimPipelineFlags a, AnimPipelineFlags b) {
		return static_cast<AnimPipelineFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}
	constexpr AnimPipelineFlags operator&(AnimPipelineFlags a, AnimPipelineFlags b) {
		return static_cast<AnimPipelineFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}



	enum struct PoolMigrationState : uint8_t { STABLE = 0, MIGRATING_TO_CPU = 1, MIGRATING_TO_GPU = 2 };

	constexpr uint8_t MAX_BLEND_LAYERS = 4;
	constexpr uint8_t MAX_IK_CHAINS = 4;

	struct PoolBlendLayer {
		uint32_t targetPoolHandle = INVALID_ANIM_POOL;
		float weight = 0.0f;
	};

	struct PoolIKEffector {
		int32_t effectorNodeID = -1;
		int32_t rootNodeID = -1;
		glm::vec3 targetPosition{0.0f};
		float weight = 0.0f;
	};

	struct AnimPoolData {
		uint64_t skeletonSignature = 0;
		uint32_t nodeCount = 0;
		uint32_t jointCount = 0;
		uint32_t frameCount = 0;

		std::string name;

		uint32_t globalJointOffset = 0xFFFFFFFF;

		uint32_t gpuBakedStaticOffset = 0xFFFFFFFF;

		std::array<uint32_t, rview::core::MAX_FRAMES_IN_FLIGHT> currentFrameIndex{};

		AnimPipelineFlags pipelineFlags = ANIM_FLAG_GPU_BAKED;
		ExecutionDomain domain = ExecutionDomain::GPU_BAKED;

		bool retainCpuData = false;
		PoolMigrationState migrationState = PoolMigrationState::STABLE;

		bool condemned = false; // report back !!

		GpuBuffer bakedAnimVBO{};
		uint32_t bakedBindlessID = 0xFFFFFFFF;

		float duration = 0.0f;
		float sampleRate = 30.0f;
		float currentTime = 0.0f;
		float playbackSpeed = 1.0f;
		bool loop = true;

		uint8_t activeIKChains = 0;
		alignas(16) std::array<PoolIKEffector, MAX_IK_CHAINS> ikEffectors{};

		float blendDuration = 0.0f;
		float currentBlendTime = 0.0f;
		alignas(16) std::array<PoolBlendLayer, MAX_BLEND_LAYERS> blendLayers{};

		std::shared_ptr<FlatSkeleton> baseSkeleton;

		std::vector<LocalTRS> localTracks;
		std::vector<glm::mat4> globalTransforms;
		std::vector<DualQuatScale> finalJointDQs;
	};

	void init_gpu_joint_buffers(rvkbucket& mvkobjs);



	struct JointAllocator {
		static constexpr uint32_t MAX_FREE_BLOCKS = 4096;

		std::array<std::atomic<uint64_t>, MAX_FREE_BLOCKS> freeBlocks{}; // 0 = empty slot
		std::atomic<uint32_t> freeCount{0};
		std::atomic<uint32_t> top{0};
		uint32_t m_capacity = 0;

		static constexpr uint64_t pack(uint32_t offset, uint32_t size) {
			return (static_cast<uint64_t>(offset) << 32) | size;
		}
		static constexpr uint32_t off(uint64_t block) {
			return static_cast<uint32_t>(block >> 32);
		}
		static constexpr uint32_t len(uint64_t block) {
			return static_cast<uint32_t>(block);
		}

		void init(uint32_t capacity) {
			for (auto& slot : freeBlocks) {
				slot.store(0, std::memory_order_relaxed);
			}

			freeCount.store(0, std::memory_order_relaxed);
			m_capacity = capacity;

			const uint32_t base = UNIVERSAL_SAFE_JOINT_OFFSET + MAX_SAFE_JOINTS;
			top.store(capacity > base ? base : capacity, std::memory_order_release);
		}

		uint32_t get_high_watermark() {
			return top.load(std::memory_order_acquire);
		}

		uint32_t allocate(uint32_t count) {
			if (count == 0)
				return UNIVERSAL_SAFE_JOINT_OFFSET;

			if (freeCount.load(std::memory_order_relaxed) != 0) {
				for (auto& slot : freeBlocks) {
					uint64_t block = slot.load(std::memory_order_acquire);

					while (block != 0 && len(block) >= count) {
						uint64_t rest = len(block) == count ? 0 : pack(off(block) + count, len(block) - count);

						if (slot.compare_exchange_weak(block, rest, std::memory_order_acq_rel, std::memory_order_acquire)) {
							if (rest == 0)
								freeCount.fetch_sub(1, std::memory_order_relaxed);

							return off(block);
						}
					}
				}
			}

			uint32_t t = top.load(std::memory_order_acquire);

			while (t <= m_capacity && count <= m_capacity - t) {
				if (top.compare_exchange_weak(t, t + count, std::memory_order_acq_rel, std::memory_order_acquire))
					return t;
			}

			return 0xFFFFFFFF; // OOM
		}

		void free(uint32_t offset, uint32_t count) {
			if (offset == 0xFFFFFFFF || offset == UNIVERSAL_SAFE_JOINT_OFFSET || count == 0)
				return;

			uint32_t lo = offset;
			uint32_t hi = offset + count;

			for (bool again = true; again;) {
				again = false;

				for (auto& slot : freeBlocks) {
					uint64_t block = slot.load(std::memory_order_acquire);

					if (block == 0 || (off(block) + len(block) != lo && off(block) != hi))
						continue;

					if (slot.compare_exchange_strong(block, 0, std::memory_order_acq_rel, std::memory_order_acquire)) {
						freeCount.fetch_sub(1, std::memory_order_relaxed);
						lo = std::min(lo, off(block));
						hi = std::max(hi, off(block) + len(block));
					}

					again = true; // look again
				}
			}

			uint32_t t = hi;

			if (top.compare_exchange_strong(t, lo, std::memory_order_acq_rel, std::memory_order_acquire))
				return;

			for (auto& slot : freeBlocks) {
				uint64_t empty = 0;

				if (slot.compare_exchange_strong(empty, pack(lo, hi - lo), std::memory_order_acq_rel, std::memory_order_acquire)) {
					freeCount.fetch_add(1, std::memory_order_relaxed);
					return;
				}
			}

			SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "joint allocator free table is full: %u joints stay unavailable", hi - lo);
		}
	};
	struct OrphanedAnimBuffer {
		GpuBuffer vbo;
		uint32_t bindlessID;
	};
	inline lfmpmc<OrphanedAnimBuffer, MAX_ANIM_POOLS> g_orphanedAnimBuffers;

	struct OrphanedCPUAnimData {
		std::vector<LocalTRS> locals;
		std::vector<glm::mat4> globals;
		std::vector<DualQuatScale> dqs;
	};

	template <typename T> inline void reap(T&& dead) {
		auto* corpse = new std::decay_t<T>(std::forward<T>(dead));

		greaper.enqueue([corpse]() noexcept { delete corpse; });
	}


	struct AnimPoolRegistry {
		static constexpr uint32_t MAX_POOLS = MAX_ANIM_POOLS;

		std::array<AnimPoolData, MAX_POOLS> pools;
		std::array<std::atomic<uint16_t>, MAX_POOLS> generations;
		std::array<std::atomic<bool>, MAX_POOLS> is_active;
		lfmpmc<uint32_t, MAX_POOLS> free_indices;
		std::atomic<uint32_t> active_count{1};
		lfmpmc<uint32_t, MAX_POOLS> unnamed;
		std::unordered_map<std::string, uint32_t> nameToHandle;
		std::array<uint32_t, MAX_POOLS> dying; // all of us are
		std::atomic<bool> full_logged{false};

		JointAllocator jointAlloc;

		AnimPoolRegistry() {
			for (uint32_t i = 0; i < MAX_POOLS; ++i) {
				generations[i].store(0, std::memory_order_relaxed);
				is_active[i].store(false, std::memory_order_relaxed);
			}

			dying.fill(INVALID_ANIM_POOL);

			for (uint32_t i = 1; i < MAX_POOLS; ++i) {
				free_indices.push(i);
			}

			AnimPoolData fallback{};
			fallback.skeletonSignature = 0;
			fallback.nodeCount = 1;
			fallback.jointCount = MAX_SAFE_JOINTS;
			fallback.frameCount = 1;
			fallback.duration = 1.0f;
			fallback.sampleRate = 30.0f;
			fallback.name = "__FALLBACK_POOL__";
			fallback.globalJointOffset = UNIVERSAL_SAFE_JOINT_OFFSET;
			fallback.pipelineFlags = ANIM_FLAG_CPU_BAKED;
			fallback.domain = ExecutionDomain::CPU_BAKED;
			fallback.bakedBindlessID = 0xFFFFFFFF;

			pools[0] = std::move(fallback);
			is_active[0].store(true, std::memory_order_release);
			nameToHandle["__FALLBACK_POOL__"] = 0;
		}

		void InitJointAllocator(uint32_t capacity) {
			jointAlloc.init(capacity);
		}

		uint32_t get_active_count() const {
			return active_count.load(std::memory_order_relaxed);
		}

		uint32_t RegisterPool(AnimPoolData&& data) {
			uint32_t index;

			if (!free_indices.pop(index)) {
				// not silent anymore
				if (!full_logged.exchange(true, std::memory_order_relaxed))
					SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "anim pool registry is full (%u pools): new pools get the fallback pool", MAX_ANIM_POOLS);

				return FALLBACK_POOL_HANDLE;
			}

			uint32_t jointOffset = jointAlloc.allocate(data.jointCount);

			if (data.domain == ExecutionDomain::CPU_BAKED || data.retainCpuData || (data.pipelineFlags & ANIM_FLAG_CPU_BAKED) != 0) {
				if (jointOffset == 0xFFFFFFFF) {
					free_indices.push(index);
					return FALLBACK_POOL_HANDLE;
				}
			}

			data.globalJointOffset = jointOffset;
			data.condemned = false;
			uint32_t handle = (static_cast<uint32_t>(generations[index].load(std::memory_order_acquire)) << 16) | index;

			pools[index] = std::move(data);
			is_active[index].store(true, std::memory_order_release);
			active_count.fetch_add(1, std::memory_order_relaxed);

			unnamed.push(handle);

			return handle;
		}

		void SyncNames() {
			uint32_t handle;

			while (unnamed.pop(handle)) {
				uint32_t index = handle & 0xFFFF;
				uint16_t gen = static_cast<uint16_t>(handle >> 16);

				if (index < MAX_POOLS && generations[index].load(std::memory_order_acquire) == gen && is_active[index].load(std::memory_order_acquire)) {
					nameToHandle[pools[index].name + "_uid" + std::to_string(handle)] = handle;
				}
			}
		}

		void DestroyPools(const uint32_t* handles, size_t count) {
			SyncNames();

			bool any = false;

			for (size_t h = 0; h < count; ++h) {
				uint32_t handle = handles[h];

				if (handle == FALLBACK_POOL_HANDLE || handle == INVALID_ANIM_POOL)
					continue;

				uint32_t index = handle & 0xFFFF;
				uint16_t gen = static_cast<uint16_t>(handle >> 16);

				if (index >= MAX_POOLS || generations[index].load(std::memory_order_acquire) != gen || !is_active[index].load(std::memory_order_acquire))
					continue;

				AnimPoolData& pool = pools[index];

				if (pool.migrationState != PoolMigrationState::STABLE) {
					pool.condemned = true;
					continue;
				}

				if (pool.bakedAnimVBO.buffer != VK_NULL_HANDLE) {
					if (!g_orphanedAnimBuffers.push({pool.bakedAnimVBO, pool.bakedBindlessID}))
						SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "orphaned anim buffer queue is full: one gpu buffer leaks");

					pool.bakedAnimVBO.buffer = VK_NULL_HANDLE;
					pool.bakedBindlessID = 0xFFFFFFFF;
				}

				if (pool.globalJointOffset != UNIVERSAL_SAFE_JOINT_OFFSET) {
					jointAlloc.free(pool.globalJointOffset, pool.jointCount);
				}

				nameToHandle.erase(pool.name + "_uid" + std::to_string(handle));

				
				if (!pool.localTracks.empty() || !pool.globalTransforms.empty() || !pool.finalJointDQs.empty()) {
					reap(OrphanedCPUAnimData{std::move(pool.localTracks), std::move(pool.globalTransforms), std::move(pool.finalJointDQs)});
				}

				pool.baseSkeleton.reset();
				pool.condemned = false;

				is_active[index].store(false, std::memory_order_release);
				generations[index].fetch_add(1, std::memory_order_acq_rel);
				active_count.fetch_sub(1, std::memory_order_relaxed);

				dying[index] = handle;
				any = true;
			}

			if (!any)
				return;

			uint32_t active_entities = g_scene.entity_count.load(std::memory_order_relaxed);

			for (uint32_t i = 0; i < active_entities; ++i) {
				uint32_t index = g_scene.poolHandles[i] & 0xFFFF;

				if (index < MAX_POOLS && dying[index] == g_scene.poolHandles[i]) {
					g_scene.poolHandles[i] = FALLBACK_POOL_HANDLE;
				}
			}

			
			for (size_t h = 0; h < count; ++h) {
				uint32_t index = handles[h] & 0xFFFF;

				if (index < MAX_POOLS && dying[index] == handles[h]) {
					dying[index] = INVALID_ANIM_POOL;
					free_indices.push(index);
				}
			}
		}

		void DestroyPool(uint32_t handle) {
			DestroyPools(&handle, 1);
		}

		const AnimPoolData* GetPool(uint32_t handle) const {
			uint32_t index = handle & 0xFFFF;
			uint16_t gen = static_cast<uint16_t>(handle >> 16);

			if (index < MAX_POOLS && generations[index].load(std::memory_order_acquire) == gen && is_active[index].load(std::memory_order_acquire)) {
				return &pools[index];
			}

			return &pools[FALLBACK_POOL_HANDLE];
		}

		AnimPoolData* GetPoolMutable(uint32_t handle) {
			uint32_t index = handle & 0xFFFF;
			uint16_t gen = static_cast<uint16_t>(handle >> 16);

			if (index < MAX_POOLS && generations[index].load(std::memory_order_acquire) == gen && is_active[index].load(std::memory_order_acquire)) {
				return &pools[index];
			}

			return nullptr;
		}
	};

	struct PoolUploadReq {
		uint32_t poolHandle;
	};
	inline lfmpmc<PoolUploadReq, MAX_ANIM_POOLS> g_poolUploadQueue;
	inline AnimPoolRegistry g_animPoolRegistry;


	inline void reserve_mega_buffers(size_t capacity) {
		g_joint_final_matrices.resize(rview::core::MAX_FRAMES_IN_FLIGHT);

		for (auto& buffer : g_joint_final_matrices) {
			buffer.resize(capacity > MAX_SAFE_JOINTS ? capacity - MAX_SAFE_JOINTS : 0);
		}

		g_animPoolRegistry.InitJointAllocator(static_cast<uint32_t>(capacity));
	}

	struct StateChangeReq {
		uint32_t dense_idx;
		uint32_t targetPool;
		AnimPipelineFlags targetFlags;

		uint64_t targetExecutionFrame;
	};

	inline lfmpmc<StateChangeReq, 4096> g_stateChangeQueue;

	inline std::vector<StateChangeReq> g_pendingStateChanges;

	struct CPUModelAsset {
		bool isSkinned = false;
		std::unique_ptr<FlatSkeleton> baseSkeleton;
		std::vector<uint32_t> defaultClips;
		GpuBuffer geometryVBO;
		std::vector<texdata> textures;
		std::vector<uint32_t> materialIDs;
		uint32_t refCount = 0;
	};

	inline std::mutex g_modelResourceMtx;
	inline std::unordered_map<uint32_t, CPUModelAsset> g_cpuModels;


	inline std::vector<glm::mat4> g_frameJointMatrices;

	inline std::unordered_map<uint32_t, std::string> g_model_filepaths;

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

	struct StagingTexture {
		size_t imageIndex;
		std::string name;
		int width = 0;
		int height = 0;
		std::vector<uint8_t> pixels;
	};

	struct StagingMaterial {
		MaterialData data;

		uint32_t localAlbedoImgIdx = 0xFFFFFFFF;
		uint32_t localNormalImgIdx = 0xFFFFFFFF;
		uint32_t localOrmImgIdx = 0xFFFFFFFF;
		uint32_t localEmissiveImgIdx = 0xFFFFFFFF;

		bool isMToon = false;
		glm::vec3 mtoonShadeColorFactor = glm::vec3(1.0f);
		uint32_t localShadeImgIdx = 0xFFFFFFFF;
		float mtoonShadingShiftFactor = 0.0f;
		float mtoonShadingToonyFactor = 0.9f;
	};

	struct StagingPrimitive {
		PrimitiveMetadata meta;
		std::vector<uint8_t> indexBytes;
		// Slots: [0]=POS, [1]=NORM, [2]=TAN, [3]=UV, [4]=JOINTS, [5]=WEIGHTS // MISSING COLOR STILL
		std::array<std::vector<uint8_t>, 6> vertexBytes;
		std::vector<uint8_t> morphBytes;
	};

	struct StagingMesh {
		std::vector<StagingPrimitive> primitives;
	};

	struct StagingModelData {
		std::string name;
		std::vector<StagingMesh> meshes;
		std::vector<StagingMaterial> materials;
		std::vector<StagingTexture> textures;

		bool isSkinned = false;
		std::unique_ptr<ParsedSkeleton> parsed_skel;
		std::vector<AnimPoolData> parsedPools;

		size_t strictAlignedTotalBytes = 0;
		uint32_t requested_instances = 1;

		glm::vec3 spawn_position = glm::vec3(0.0f);
		bool skipui = false;
		void (*demo_modifier)(uint32_t idx, uint32_t total, struct Entity e, glm::vec3 origin) = nullptr;
		uint32_t dropID = 0xFFFFFFFF;
		uint32_t assignedModelID = 0xFFFFFFFF;
		uint32_t defaultPool = 0xFFFFFFFF;
		uint32_t loadID = 0xFFFFFFFF;
		//temp
		std::vector<VkBuffer> buffersToAcquire;
		std::vector<VkImage> imagesToAcquire;
	};

	std::string get_glb_json_chunk(std::string_view filepath);
	StagingModelData parse_model_to_staging(std::string_view filepath, uint32_t dropID = 0xFFFFFFFF);

	uint32_t commit_staging_to_vulkan(rvkbucket& mvkobjs, StagingModelData& staging, std::vector<uint32_t>& uploadedTextureBindlessIDs);


	void upload_joint_matrices(rvkbucket& mvkobjs, uint32_t read_frame);


	struct Entity {
		uint32_t handle = 0xFFFFFFFF;

		inline uint32_t index() const {
			return handle & 0xFFFFF;
		}
		inline uint32_t generation() const {
			return (handle >> 20) & 0xFFF;
		}
		inline bool is_valid() const {
			return handle != 0xFFFFFFFF;
		}
	};

	
	struct Registry {
		static constexpr uint32_t MAX_ENTITIES = SceneData::MAX_ENTITIES;
		std::array<uint32_t, MAX_ENTITIES> sparse;
		std::vector<uint32_t> free_indices;

		std::array<uint32_t, MAX_ENTITIES> dense_to_sparse;
		std::array<uint32_t, MAX_ENTITIES> generations;

		alignas(64) std::array<bool, MAX_ENTITIES> is_visible{};
		alignas(64) std::array<uint32_t, MAX_ENTITIES> attach_parent_entity{};
		alignas(64) std::array<int32_t, MAX_ENTITIES> attach_parent_bone{};
		alignas(64) std::array<glm::vec3, MAX_ENTITIES> attach_offset_pos{};
		alignas(64) std::array<glm::quat, MAX_ENTITIES> attach_offset_rot{};

		std::vector<float> flat_morph_weights;
		alignas(64) std::array<uint32_t, MAX_ENTITIES> morph_weight_offsets{};
		alignas(64) std::array<uint32_t, MAX_ENTITIES> morph_weight_counts{};

		Registry() {
			sparse.fill(0xFFFFFFFF);
			dense_to_sparse.fill(0xFFFFFFFF);
			generations.fill(0);
			is_visible.fill(true);
			attach_parent_entity.fill(0xFFFFFFFF);
			attach_parent_bone.fill(-1);
		}

		Entity create_entity(uint32_t modelID) {
			uint32_t entity_idx;

			if (!free_indices.empty()) {
				entity_idx = free_indices.back();
				free_indices.pop_back();
			} else {
				entity_idx = g_scene.entity_count.load(std::memory_order_relaxed);

				if (entity_idx >= MAX_ENTITIES)
					return Entity{};
			}

			uint32_t dense_idx = g_scene.entity_count.fetch_add(1, std::memory_order_relaxed);
			sparse[entity_idx] = dense_idx;
			dense_to_sparse[dense_idx] = entity_idx;

			uint32_t current_gen = generations[entity_idx];
			Entity ent = {(current_gen << 20) | entity_idx};

			g_scene.worldPositions[dense_idx] = glm::vec3(0.0f);
			g_scene.rotations[dense_idx] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			g_scene.scales[dense_idx] = glm::vec3(1.0f);
			g_scene.modelIDs[dense_idx] = modelID;
			g_scene.poolHandles[dense_idx] = FALLBACK_POOL_HANDLE;

			is_visible[dense_idx] = true;

			uint32_t target_count = 0;

			if (modelID != 0xFFFFFFFF && modelID < g_assets.models.size()) {
				const ModelMetadata& meta = g_assets.models[modelID];

				if (meta.primitiveCount > 0) {
					target_count = g_assets.primitives[meta.firstPrimitiveIndex].targetCount;
				}
			}

			morph_weight_offsets[dense_idx] = static_cast<uint32_t>(flat_morph_weights.size());
			morph_weight_counts[dense_idx] = target_count;
			flat_morph_weights.insert(flat_morph_weights.end(), target_count, 0.0f);

			return ent;
		}

		bool is_valid(Entity e) const {
			return e.is_valid() && e.index() < MAX_ENTITIES && generations[e.index()] == e.generation() && sparse[e.index()] != 0xFFFFFFFF;
		}

		void destroy_entity(Entity e) {
			if (!is_valid(e))
				return;

			uint32_t deleted_dense_idx = sparse[e.index()];

			uint32_t del_off = morph_weight_offsets[deleted_dense_idx];
			uint32_t del_cnt = morph_weight_counts[deleted_dense_idx];

			if (del_cnt > 0) {
				flat_morph_weights.erase(flat_morph_weights.begin() + del_off, flat_morph_weights.begin() + del_off + del_cnt);
				uint32_t active_count = g_scene.entity_count.load(std::memory_order_relaxed);

				for (uint32_t i = 0; i < active_count; ++i) {
					if (morph_weight_offsets[i] > del_off) {
						morph_weight_offsets[i] -= del_cnt;
					}
				}
			}

			uint32_t last_dense_idx = g_scene.entity_count.load(std::memory_order_relaxed) - 1;

			if (deleted_dense_idx != last_dense_idx) {
				uint32_t last_entity_idx = dense_to_sparse[last_dense_idx];

				g_scene.worldPositions[deleted_dense_idx] = g_scene.worldPositions[last_dense_idx];
				g_scene.rotations[deleted_dense_idx] = g_scene.rotations[last_dense_idx];
				g_scene.scales[deleted_dense_idx] = g_scene.scales[last_dense_idx];
				g_scene.modelIDs[deleted_dense_idx] = g_scene.modelIDs[last_dense_idx];
				g_scene.poolHandles[deleted_dense_idx] = g_scene.poolHandles[last_dense_idx];

				is_visible[deleted_dense_idx] = is_visible[last_dense_idx];
				morph_weight_offsets[deleted_dense_idx] = morph_weight_offsets[last_dense_idx];
				morph_weight_counts[deleted_dense_idx] = morph_weight_counts[last_dense_idx];
				attach_parent_entity[deleted_dense_idx] = attach_parent_entity[last_dense_idx];
				attach_parent_bone[deleted_dense_idx] = attach_parent_bone[last_dense_idx];
				attach_offset_pos[deleted_dense_idx] = attach_offset_pos[last_dense_idx];
				attach_offset_rot[deleted_dense_idx] = attach_offset_rot[last_dense_idx];

				sparse[last_entity_idx] = deleted_dense_idx;
				dense_to_sparse[deleted_dense_idx] = last_entity_idx;
			}

			sparse[e.index()] = 0xFFFFFFFF;
			generations[e.index()]++;
			free_indices.push_back(e.index());

			g_scene.entity_count.fetch_sub(1, std::memory_order_relaxed);
		}

		inline uint32_t get_dense_index(Entity e) const {
			return sparse[e.index()];
		}
		inline glm::vec3& position(Entity e) {
			return g_scene.worldPositions[sparse[e.index()]];
		}
		inline glm::quat& rotation(Entity e) {
			return g_scene.rotations[sparse[e.index()]];
		}
		inline glm::vec3& scale(Entity e) {
			return g_scene.scales[sparse[e.index()]];
		}
	};

	inline Registry g_registry;

	inline void UpdateAttachments() {
		ZoneScopedN("Update Attachments");
		uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

		for (uint32_t i = 0; i < active_instances; ++i) {
			if (!g_registry.is_visible[i])
				continue;

			uint32_t parent_idx = g_registry.attach_parent_entity[i];
			int32_t parent_bone = g_registry.attach_parent_bone[i];

			if (parent_idx != 0xFFFFFFFF && parent_bone >= 0) {
				uint32_t parent_pool_handle = g_scene.poolHandles[parent_idx];
				const auto* parent_pool = g_animPoolRegistry.GetPool(parent_pool_handle);

				glm::mat4 bone_model_mat(1.0f);

				if (parent_pool && parent_bone < static_cast<int32_t>(parent_pool->nodeCount)) {
					bone_model_mat = parent_pool->globalTransforms[parent_bone];
				}

				glm::mat4 offset_mat =
					glm::translate(glm::mat4(1.0f), g_registry.attach_offset_pos[i]) * glm::mat4_cast(g_registry.attach_offset_rot[i]);
				glm::mat4 final_attachment_mat = bone_model_mat * offset_mat;

				glm::vec3 parent_world_pos = g_scene.worldPositions[parent_idx];
				glm::quat parent_world_rot = g_scene.rotations[parent_idx];
				glm::vec3 parent_scale = g_scene.scales[parent_idx];

				glm::mat4 parent_world_mat =
					glm::translate(glm::mat4(1.0f), parent_world_pos) * glm::mat4_cast(parent_world_rot) * glm::scale(glm::mat4(1.0f), parent_scale);
				glm::mat4 absolute_world_mat = parent_world_mat * final_attachment_mat;

				g_scene.worldPositions[i] = glm::vec3(absolute_world_mat[3]);

				glm::vec3 scale, translation, skew;
				glm::quat rotation;
				glm::vec4 perspective;
				glm::decompose(absolute_world_mat, scale, rotation, translation, skew, perspective);

				g_scene.rotations[i] = glm::conjugate(rotation);
				g_scene.scales[i] = scale;
			}
		}
	}


	enum struct ParseStep { parsing, baking, done, cancelled };

	struct ProgressUpdate {
		uint32_t requestID;
		ParseStep step;
		char message[128]; // HARDCODED
	};

	// queue just for telemetry might delete
	inline lfmpmc<ProgressUpdate, 128> g_progress_queue;

	struct PoolMigrationReq {
		uint32_t poolHandle;
		ExecutionDomain targetDomain;
	};
	inline lfmpmc<PoolMigrationReq, MAX_ANIM_POOLS> g_poolMigrationQueue;

	inline void ProcessDeferredStateChanges(uint64_t current_engine_frame) {
		ZoneScopedN("Process State Changes");

		StateChangeReq req;

		while (g_stateChangeQueue.pop(req)) {
			g_pendingStateChanges.push_back(req);
		}

		for (int i = static_cast<int>(g_pendingStateChanges.size()) - 1; i >= 0; --i) {
			if (current_engine_frame >= g_pendingStateChanges[i].targetExecutionFrame) {
				const auto& readyReq = g_pendingStateChanges[i];

				g_scene.poolHandles[readyReq.dense_idx] = readyReq.targetPool;

				g_pendingStateChanges[i] = g_pendingStateChanges.back();
				g_pendingStateChanges.pop_back();
			}
		}
	}
	inline void EvaluateFK_And_CompressDQS(AnimPoolData* pool, const LocalTRS* locals, const uint32_t& frameidx) {
		if (!pool->baseSkeleton)
			return;

		const int32_t* parents = pool->baseSkeleton->parentIndices.data();
		const uint32_t* jointToNode = pool->baseSkeleton->jointToNodeMap.data();
		const glm::mat4* inverseBinds = pool->baseSkeleton->inverseBindMatrices.data();
		glm::mat4* outGlobals = pool->globalTransforms.data();

		uint32_t b_count = pool->nodeCount;
		uint32_t j_count = pool->jointCount;
		uint32_t offset = pool->globalJointOffset;

		for (uint32_t b = 0; b < b_count; ++b) {
			glm::mat4 localMat =
				glm::translate(glm::mat4(1.0f), locals[b].T) * glm::mat4_cast(locals[b].R) * glm::scale(glm::mat4(1.0f), locals[b].S);
			int32_t parent = parents[b];

			if (parent >= 0) {
				outGlobals[b] = outGlobals[parent] * localMat;
			} else {
				outGlobals[b] = localMat;
			}
		}

		for (uint32_t j = 0; j < j_count; ++j) {
			uint32_t globalNodeIdx = jointToNode[j];
			glm::mat4 finalMat = outGlobals[globalNodeIdx] * inverseBinds[j];
			g_joint_final_matrices[frameidx][(offset - MAX_SAFE_JOINTS) + j] = Mat4ToDualQuatScale(finalMat);
		}
	}

	
	inline uint32_t PoolFrameIndex(const AnimPoolData& pool) {
		float timePos = pool.currentTime;
		bool loop = pool.loop;

		float adjustedTime = loop ? std::fmod(timePos, pool.duration) : std::min(timePos, pool.duration);

		if (adjustedTime < 0.0f)
			adjustedTime += pool.duration;

		uint32_t frameIndex = static_cast<uint32_t>(adjustedTime * pool.sampleRate);

		if (frameIndex >= pool.frameCount)
			frameIndex = pool.frameCount - 1;

		return frameIndex;
	}

	template <uint8_t Flags> void ProcessPoolChunk(const uint32_t* poolHandles, size_t count, float deltaTime, uint32_t write_frame) {
		ZoneScopedN("ProcessPoolChunk");

		for (size_t i = 0; i < count; ++i) {
			uint32_t poolHandle = poolHandles[i];
			AnimPoolData& pool = g_animPoolRegistry.pools[poolHandle];

			if (pool.frameCount == 0 || pool.duration <= 0.0f)
				continue;

			pool.currentTime += deltaTime * pool.playbackSpeed;
			uint32_t frameIndex = PoolFrameIndex(pool);

			if constexpr ((Flags & ANIM_FLAG_CPU_BAKED) != 0) {
				
				pool.currentFrameIndex[write_frame] = frameIndex;

				
				if ((static_cast<size_t>(frameIndex) + 1) * pool.jointCount > pool.finalJointDQs.size())
					continue;

				const DualQuatScale* frameDQs = &pool.finalJointDQs[frameIndex * pool.jointCount];
				std::memcpy(&g_joint_final_matrices[write_frame][pool.globalJointOffset - MAX_SAFE_JOINTS],
							frameDQs,
							pool.jointCount * sizeof(DualQuatScale));
				continue;
			}

			else if constexpr ((Flags & ANIM_FLAG_GPU_BAKED) != 0) {
				pool.currentFrameIndex[write_frame] = frameIndex;
				continue;
			}

			else if constexpr ((Flags & ANIM_FLAG_CPU_EVAL) != 0) {

			}

			else if constexpr ((Flags & ANIM_FLAG_GPU_EVAL) != 0) {
			}


			if constexpr ((Flags & ANIM_FLAG_BLENDING) != 0) {
				if (pool.retainCpuData) {}
			}

			if constexpr ((Flags & ANIM_FLAG_IK) != 0) {
				if (pool.retainCpuData) {}
			}
		}
	}

	
	
	inline bool AnimQuiet() {
		for (auto& pending : g_anim_jobs_pending) {
			if (pending.load(std::memory_order_acquire) != 0)
				return false;
		}

		return true;
	}

	
	
	inline void MarkPoolsInUse() {
		g_pool_used_valid = false;

		if (g_cpu_baked_pools == 0)
			return;

		ZoneScopedN("Mark Pools In Use");

		g_pool_used.fill(0);
		uint32_t active_entities = g_scene.entity_count.load(std::memory_order_relaxed);

		for (uint32_t i = 0; i < active_entities; ++i) {
			uint32_t index = g_scene.poolHandles[i] & 0xFFFF;

			if (index < MAX_ANIM_POOLS)
				g_pool_used[index] = 1;
		}

		g_pool_used_valid = true;
	}

	
	
	
	inline void CatchUpPools(uint32_t read_frame) {
		if (!g_pool_used_valid || !AnimQuiet())
			return;

		for (uint32_t p = 1; p < MAX_ANIM_POOLS; ++p) {
			if (!g_pool_used[p] || g_pool_copied[read_frame][p])
				continue;

			if (!g_animPoolRegistry.is_active[p].load(std::memory_order_acquire))
				continue;

			const AnimPoolData& pool = g_animPoolRegistry.pools[p];

			if (static_cast<uint8_t>(pool.pipelineFlags) != ANIM_FLAG_CPU_BAKED || pool.frameCount == 0 || pool.duration <= 0.0f || pool.jointCount == 0)
				continue;

			if (pool.globalJointOffset < MAX_SAFE_JOINTS || pool.globalJointOffset == 0xFFFFFFFF)
				continue;

			size_t first = static_cast<size_t>(PoolFrameIndex(pool)) * pool.jointCount;

			if (first + pool.jointCount > pool.finalJointDQs.size())
				continue;

			std::memcpy(&g_joint_final_matrices[read_frame][pool.globalJointOffset - MAX_SAFE_JOINTS],
						&pool.finalJointDQs[first],
						pool.jointCount * sizeof(DualQuatScale));
			g_pool_copied[read_frame][p] = 1;
		}
	}

	inline void DispatchAnimationJobs(float deltaTime, uint32_t write_frame) {
		ZoneScoped;

		if (g_animPoolRegistry.get_active_count() <= 1)
			return;

		
		
		tlarena& arena = g_anim_arenas[write_frame];
		arena.reset();

		constexpr uint8_t MAX_FLAGS = 64;
		uint32_t* buckets[MAX_FLAGS] = {nullptr};
		uint32_t bucket_counts[MAX_FLAGS] = {0};

		struct entry {
			uint32_t pool;
			uint32_t flag;
		};

		auto* entries = static_cast<entry*>(arena.allocate(MAX_ANIM_POOLS * sizeof(entry), alignof(entry)));
		uint32_t entry_count = 0;
		uint32_t cpu_baked = 0;

		g_pool_copied[write_frame].fill(0);

		for (uint32_t p = 1; p < AnimPoolRegistry::MAX_POOLS; ++p) {
			if (!g_animPoolRegistry.is_active[p].load(std::memory_order_acquire))
				continue;

			AnimPoolData& pool = g_animPoolRegistry.pools[p];

			if (pool.frameCount == 0)
				continue;

			uint8_t flagMask = static_cast<uint8_t>(pool.pipelineFlags);

			if (flagMask == ANIM_FLAG_CPU_BAKED) {
				++cpu_baked;

				if (g_pool_used_valid && !g_pool_used[p]) {
					
					
					flagMask = ANIM_FLAG_GPU_BAKED;
				} else {
					g_pool_copied[write_frame][p] = 1;
				}
			}

			if (flagMask < MAX_FLAGS) {
				entries[entry_count++] = {p, flagMask};
				++bucket_counts[flagMask];
			}
		}

		g_cpu_baked_pools = cpu_baked;

		if (entry_count == 0)
			return;

		
		for (uint8_t i = 0; i < MAX_FLAGS; ++i) {
			if (bucket_counts[i] > 0) {
				buckets[i] = static_cast<uint32_t*>(arena.allocate(bucket_counts[i] * sizeof(uint32_t), alignof(uint32_t)));
				bucket_counts[i] = 0;
			}
		}

		for (uint32_t e = 0; e < entry_count; ++e) {
			buckets[entries[e].flag][bucket_counts[entries[e].flag]++] = entries[e].pool;
		}

		g_anim_jobs_pending[write_frame].store(0, std::memory_order_relaxed);

		// i just dont know which is better until i write my custom allocator

		auto dispatchDomain = [&](uint8_t flag, auto func) {
			if (bucket_counts[flag] > 0) {
				g_anim_jobs_pending[write_frame].fetch_add(1, std::memory_order_relaxed);
				auto* bucket = buckets[flag];
				auto count = bucket_counts[flag];
				const uint64_t tq = mnow();
				gjobs.enqueue([bucket, count, deltaTime, write_frame, func, tq]() noexcept {
					const uint64_t t0 = mnow();
					func(bucket, count, deltaTime, write_frame);
					gm.janim(t0 - tq, mnow() - t0);
					g_anim_jobs_pending[write_frame].fetch_sub(1, std::memory_order_release);
				});
			}
		};

		dispatchDomain(ANIM_FLAG_CPU_BAKED, ProcessPoolChunk<ANIM_FLAG_CPU_BAKED>);
		dispatchDomain(ANIM_FLAG_GPU_BAKED, ProcessPoolChunk<ANIM_FLAG_GPU_BAKED>);
	}

	struct PoolPurgeReq {
		uint32_t poolHandle;
	};
	inline lfmpmc<PoolPurgeReq, MAX_ANIM_POOLS> g_poolPurgeQueue;

	inline void PurgePoolCPUDataAsync(AnimPoolData& pool) {
		reap(OrphanedCPUAnimData{std::move(pool.localTracks), std::move(pool.globalTransforms), std::move(pool.finalJointDQs)});
	}
	inline std::mutex g_transfer_mtx;

	
	
	struct ChunkUploader {
		static constexpr size_t CHUNK_SIZE = 16 * 1024 * 1024;

		rvkbucket& mvkobjs;
		size_t capacity = 0;
		size_t used = 0;
		bool recording = false;
		uint32_t transferQFamily = 0;
		uint32_t graphicsQFamily = 0;
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VmaAllocation stagingAlloc = VK_NULL_HANDLE;
		VmaAllocationInfo stagingAllocInfo{};
		VkCommandPool transferPool = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkFence chunkFence = VK_NULL_HANDLE;

		ChunkUploader(const ChunkUploader&) = delete;
		ChunkUploader& operator=(const ChunkUploader&) = delete;

		
		explicit ChunkUploader(rvkbucket& objs, size_t stagingSize = CHUNK_SIZE)
			: mvkobjs(objs), capacity(std::min(std::max<size_t>(stagingSize, 64 * 1024), CHUNK_SIZE)) {
			VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			binfo.size = capacity;
			binfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

			VmaAllocationCreateInfo vmaallocinfo{};
			vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
			vmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

			vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo, &stagingBuffer, &stagingAlloc, &stagingAllocInfo);

			transferQFamily = mvkobjs.vkdevice.get_queue_index(vkb::QueueType::transfer).value();
			graphicsQFamily = mvkobjs.vkdevice.get_queue_index(vkb::QueueType::graphics).value();

			VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
			poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			poolInfo.queueFamilyIndex = transferQFamily;
			vkCreateCommandPool(mvkobjs.vkdevice.device, &poolInfo, nullptr, &transferPool);

			VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			allocInfo.commandPool = transferPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;
			vkAllocateCommandBuffers(mvkobjs.vkdevice.device, &allocInfo, &cmd);

			VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			vkCreateFence(mvkobjs.vkdevice.device, &fenceInfo, nullptr, &chunkFence);
		}

		void begin() {
			if (recording)
				return;

			VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(cmd, &beginInfo);
			recording = true;
		}

		
		void flush() {
			if (!recording)
				return;

			vmaFlushAllocation(mvkobjs.alloc, stagingAlloc, 0, used);
			vkEndCommandBuffer(cmd);

			VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;

			{
				std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
				vkQueueSubmit(mvkobjs.transferQ, 1, &submitInfo, chunkFence);
			}
			vkWaitForFences(mvkobjs.vkdevice.device, 1, &chunkFence, VK_TRUE, UINT64_MAX);
			vkResetFences(mvkobjs.vkdevice.device, 1, &chunkFence);
			vkResetCommandBuffer(cmd, 0);

			recording = false;
			used = 0;
		}

		
		
		void upload(GpuBuffer& dstBuffer, const void* srcData, size_t totalSize) {
			if (totalSize == 0)
				return;

			size_t offset = 0;

			while (offset < totalSize) {
				if (used == capacity)
					flush();

				size_t currentChunk = std::min(capacity - used, totalSize - offset);

				std::memcpy(static_cast<uint8_t*>(stagingAllocInfo.pMappedData) + used, static_cast<const uint8_t*>(srcData) + offset, currentChunk);

				begin();

				VkBufferCopy copyRegion{};
				copyRegion.srcOffset = used;
				copyRegion.dstOffset = offset;
				copyRegion.size = currentChunk;
				vkCmdCopyBuffer(cmd, stagingBuffer, dstBuffer.buffer, 1, &copyRegion);

				used += currentChunk;
				offset += currentChunk;
			}

			if (transferQFamily != graphicsQFamily) {
				
				VkBufferMemoryBarrier2 release{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
				release.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				release.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
				release.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
				release.dstAccessMask = 0;
				release.srcQueueFamilyIndex = transferQFamily;
				release.dstQueueFamilyIndex = graphicsQFamily;
				release.buffer = dstBuffer.buffer;
				release.offset = 0;
				release.size = VK_WHOLE_SIZE;

				VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
				dep.bufferMemoryBarrierCount = 1;
				dep.pBufferMemoryBarriers = &release;
				vkCmdPipelineBarrier2(cmd, &dep);
			}
		}

		~ChunkUploader() {
			flush();

			vkDestroyFence(mvkobjs.vkdevice.device, chunkFence, nullptr);
			vkFreeCommandBuffers(mvkobjs.vkdevice.device, transferPool, 1, &cmd);
			vkDestroyCommandPool(mvkobjs.vkdevice.device, transferPool, nullptr);
			vmaDestroyBuffer(mvkobjs.alloc, stagingBuffer, stagingAlloc);
		}
	};

	
	inline void async_chunked_upload(rvkbucket& mvkobjs, GpuBuffer& dstBuffer, const void* srcData, size_t totalSize) {
		if (totalSize == 0)
			return;

		ChunkUploader uploader(mvkobjs, totalSize);
		uploader.upload(dstBuffer, srcData, totalSize);
	}
}

inline lfmpmc<StagingBelt*, 16> g_worker_belts; // magic

struct AsyncTransferContext {
	rvkbucket& mvkobjs;
	StagingBelt* belt;
	VkCommandBuffer cmd;
	VkFence fence;
	VkQueue targetQueue;

	void flush_and_reset() {
		vkEndCommandBuffer(cmd);

		VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmd;

		{
			std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
			vkQueueSubmit(targetQueue, 1, &submit, fence);
		}

		vkWaitForFences(mvkobjs.vkdevice.device, 1, &fence, VK_TRUE, UINT64_MAX);
		vkResetFences(mvkobjs.vkdevice.device, 1, &fence);

		belt->reset();

		VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &beginInfo);
	}

	VkDeviceSize safe_reserve(VkDeviceSize size) {
		VkDeviceSize offset = belt->reserve(size);
		if (offset == VK_WHOLE_SIZE) {
			flush_and_reset();
			offset = belt->reserve(size);
			if (offset == VK_WHOLE_SIZE) {
				// i can probably find a way to fragment the single asset
				throw std::runtime_error("single asset chunk exceeds belt capacity X_X");
			}
		}
		return offset;
	}
};
