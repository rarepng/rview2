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

enum class AnimMode : uint8_t {
	Baked   = 0,
	Hybrid  = 1,
	Dynamic = 2,
	Hero = 3
};


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

constexpr uint32_t INVALID_ANIM_POOL = 0xFFFFFFFF;
constexpr uint32_t FALLBACK_POOL_HANDLE = 0;
constexpr uint32_t UNIVERSAL_SAFE_JOINT_OFFSET = 0;
constexpr uint32_t MAX_SAFE_JOINTS = 512;

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
	ANIM_FLAG_NONE          = 0,

	ANIM_FLAG_CPU_BAKED     = 1 << 0,
	ANIM_FLAG_GPU_BAKED     = 1 << 1,
	ANIM_FLAG_CPU_EVAL      = 1 << 2,
	ANIM_FLAG_GPU_EVAL      = 1 << 3,

	ANIM_FLAG_BLENDING      = 1 << 4,
	ANIM_FLAG_IK            = 1 << 5
};

enum struct ExecutionDomain : uint8_t {
	CPU_BAKED = 0,
	GPU_BAKED = 1,
	CPU_EVAL  = 2,
	GPU_EVAL  = 3
};

constexpr AnimPipelineFlags operator|(AnimPipelineFlags a, AnimPipelineFlags b) {
	return static_cast<AnimPipelineFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr AnimPipelineFlags operator&(AnimPipelineFlags a, AnimPipelineFlags b) {
	return static_cast<AnimPipelineFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}



enum struct PoolMigrationState : uint8_t {
	STABLE = 0,
	MIGRATING_TO_CPU = 1,
	MIGRATING_TO_GPU = 2
};

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



class JointAllocator {
	struct Block {
		uint32_t offset;
		uint32_t size;
	};
	std::vector<Block> freeBlocks;
	std::mutex mtx;
	uint32_t m_capacity = 0;

public:
	void init(uint32_t capacity) {
		std::lock_guard<std::mutex> lock(mtx);
		freeBlocks.clear();
		m_capacity = capacity;

		if (capacity > UNIVERSAL_SAFE_JOINT_OFFSET + MAX_SAFE_JOINTS) {
			freeBlocks.push_back({UNIVERSAL_SAFE_JOINT_OFFSET + MAX_SAFE_JOINTS,
			                      capacity - (UNIVERSAL_SAFE_JOINT_OFFSET + MAX_SAFE_JOINTS)});
		}
	}

	uint32_t get_high_watermark() {
		std::lock_guard<std::mutex> lock(mtx);

		if (freeBlocks.empty()) return m_capacity;

		const auto& lastBlock = freeBlocks.back();

		if (lastBlock.offset + lastBlock.size == m_capacity) {
			return lastBlock.offset;
		}

		return m_capacity;
	}

	uint32_t allocate(uint32_t count) {
		if (count == 0) return UNIVERSAL_SAFE_JOINT_OFFSET;

		std::lock_guard<std::mutex> lock(mtx);

		for (auto it = freeBlocks.begin(); it != freeBlocks.end(); ++it) {
			if (it->size >= count) {
				uint32_t allocated_offset = it->offset;

				if (it->size == count) {
					freeBlocks.erase(it);
				} else {
					it->offset += count;
					it->size -= count;
				}

				return allocated_offset;
			}
		}

		return 0xFFFFFFFF; // OOM
	}

	void free(uint32_t offset, uint32_t count) {
		if (offset == 0xFFFFFFFF || offset == UNIVERSAL_SAFE_JOINT_OFFSET || count == 0) return;

		std::lock_guard<std::mutex> lock(mtx);
		freeBlocks.push_back({offset, count});

		std::sort(freeBlocks.begin(), freeBlocks.end(), [](const Block & a, const Block & b) {
			return a.offset < b.offset;
		});

		for (size_t i = 0; i < freeBlocks.size() - 1; ) {
			if (freeBlocks[i].offset + freeBlocks[i].size == freeBlocks[i + 1].offset) {
				freeBlocks[i].size += freeBlocks[i + 1].size;
				freeBlocks.erase(freeBlocks.begin() + i + 1);
			} else {
				i++;
			}
		}
	}
};
struct OrphanedAnimBuffer {
	GpuBuffer vbo;
	uint32_t bindlessID;
};
inline LockFreeMPMC<OrphanedAnimBuffer, 256> g_orphanedAnimBuffers;


struct AnimPoolRegistry {
	std::mutex mtx;
	static constexpr uint32_t MAX_POOLS = 4096;

	std::array<AnimPoolData, MAX_POOLS> pools;
	std::array<uint16_t, MAX_POOLS> generations;
	std::array<bool, MAX_POOLS> is_active;
	std::vector<uint32_t> free_indices;
	std::unordered_map<std::string, uint32_t> nameToHandle;

	JointAllocator jointAlloc;

	AnimPoolRegistry() {
		generations.fill(0);
		is_active.fill(false);
		free_indices.reserve(MAX_POOLS);

		for (uint32_t i = MAX_POOLS - 1; i >= 1; --i) {
			free_indices.push_back(i);
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
		is_active[0] = true;
		nameToHandle["__FALLBACK_POOL__"] = 0;
	}

	void InitJointAllocator(uint32_t capacity) {
		jointAlloc.init(capacity);
	}

	uint32_t get_active_count() const {
		return MAX_POOLS - static_cast<uint32_t>(free_indices.size());
	}

	uint32_t RegisterPool(AnimPoolData&& data) {
		std::lock_guard<std::mutex> lock(mtx);

		if (free_indices.empty()) return FALLBACK_POOL_HANDLE;

		uint32_t jointOffset = jointAlloc.allocate(data.jointCount);

		if (data.domain == ExecutionDomain::CPU_BAKED || data.retainCpuData || (data.pipelineFlags & ANIM_FLAG_CPU_BAKED) != 0) {
			jointOffset = jointAlloc.allocate(data.jointCount);

			if (jointOffset == 0xFFFFFFFF) {
				return FALLBACK_POOL_HANDLE;
			}
		}

		uint32_t index = free_indices.back();
		free_indices.pop_back();

		data.globalJointOffset = jointOffset;
		uint32_t handle = (static_cast<uint32_t>(generations[index]) << 16) | index;

		std::string unique_name = data.name + "_uid" + std::to_string(handle);
		nameToHandle[unique_name] = handle;

		pools[index] = std::move(data);
		is_active[index] = true;

		return handle;
	}

	void DestroyPool(uint32_t handle) {
		if (handle == FALLBACK_POOL_HANDLE || handle == INVALID_ANIM_POOL) return;

		uint32_t index = handle & 0xFFFF;
		uint16_t gen = static_cast<uint16_t>(handle >> 16);

		std::lock_guard<std::mutex> lock(mtx);

		if (index >= MAX_POOLS || generations[index] != gen || !is_active[index]) return;

		if (pools[index].bakedAnimVBO.buffer != VK_NULL_HANDLE) {
			g_orphanedAnimBuffers.push({pools[index].bakedAnimVBO, pools[index].bakedBindlessID});
			pools[index].bakedAnimVBO.buffer = VK_NULL_HANDLE;
			pools[index].bakedBindlessID = 0xFFFFFFFF;
		}

		if (pools[index].globalJointOffset != UNIVERSAL_SAFE_JOINT_OFFSET) {
			jointAlloc.free(pools[index].globalJointOffset, pools[index].jointCount);
		}

		uint32_t active_entities = g_scene.entity_count.load(std::memory_order_relaxed);

		for (uint32_t i = 0; i < active_entities; ++i) {
			if (g_scene.poolHandles[i] == handle) {
				g_scene.poolHandles[i] = FALLBACK_POOL_HANDLE;
			}
		}

		std::string unique_name = pools[index].name + "_uid" + std::to_string(handle);
		nameToHandle.erase(unique_name);

		is_active[index] = false;
		generations[index]++;
		free_indices.push_back(index);
	}

	const AnimPoolData* GetPool(uint32_t handle) const {
		uint32_t index = handle & 0xFFFF;
		uint16_t gen = static_cast<uint16_t>(handle >> 16);

		if (index < MAX_POOLS && generations[index] == gen && is_active[index]) {
			return &pools[index];
		}

		return &pools[FALLBACK_POOL_HANDLE];
	}

	AnimPoolData* GetPoolMutable(uint32_t handle) {
		uint32_t index = handle & 0xFFFF;
		uint16_t gen = static_cast<uint16_t>(handle >> 16);

		if (index < MAX_POOLS && generations[index] == gen && is_active[index]) {
			return &pools[index];
		}

		return nullptr;
	}
};

struct PoolUploadReq {
	uint32_t poolHandle;
};
inline LockFreeMPMC<PoolUploadReq, 256> g_poolUploadQueue;
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

inline LockFreeMPMC<StateChangeReq, 4096> g_stateChangeQueue;

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

	uint32_t localAlbedoImgIdx   = 0xFFFFFFFF;
	uint32_t localNormalImgIdx   = 0xFFFFFFFF;
	uint32_t localOrmImgIdx      = 0xFFFFFFFF;
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
	uint32_t dropID = 0xFFFFFFFF;
	void (*demo_modifier)(uint32_t idx, uint32_t total, struct Entity e, glm::vec3 origin) = nullptr;
};

std::string get_glb_json_chunk(std::string_view filepath);
StagingModelData parse_model_to_staging(std::string_view filepath, uint32_t dropID = 0xFFFFFFFF);

uint32_t commit_staging_to_vulkan(rvkbucket& mvkobjs, VkCommandBuffer cmd, StagingModelData& staging,
                                  std::vector<uint32_t>& uploadedTextureBindlessIDs);



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

class Registry {
private:
	static constexpr uint32_t MAX_ENTITIES = SceneData::MAX_ENTITIES;
	std::array<uint32_t, MAX_ENTITIES> sparse;
	std::vector<uint32_t> free_indices;
	std::mutex registry_mtx;

public:
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
		std::lock_guard<std::mutex> lock(registry_mtx);

		uint32_t entity_idx;

		if (!free_indices.empty()) {
			entity_idx = free_indices.back();
			free_indices.pop_back();
		} else {
			entity_idx = g_scene.entity_count.load(std::memory_order_relaxed);

			if (entity_idx >= MAX_ENTITIES) return Entity{};
		}

		uint32_t dense_idx = g_scene.entity_count.fetch_add(1, std::memory_order_relaxed);
		sparse[entity_idx] = dense_idx;
		dense_to_sparse[dense_idx] = entity_idx;

		uint32_t current_gen = generations[entity_idx];
		Entity ent = { (current_gen << 20) | entity_idx };

		g_scene.worldPositions[dense_idx] = glm::vec3(0.0f);
		g_scene.rotations[dense_idx]      = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		g_scene.scales[dense_idx]         = glm::vec3(1.0f);
		g_scene.modelIDs[dense_idx]       = modelID;
		g_scene.poolHandles[dense_idx]    = FALLBACK_POOL_HANDLE;

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
		return e.is_valid() &&
		       e.index() < MAX_ENTITIES &&
		       generations[e.index()] == e.generation() &&
		       sparse[e.index()] != 0xFFFFFFFF;
	}

	void destroy_entity(Entity e) {
		std::lock_guard<std::mutex> lock(registry_mtx);

		if (!is_valid(e)) return;

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
			g_scene.rotations[deleted_dense_idx]      = g_scene.rotations[last_dense_idx];
			g_scene.scales[deleted_dense_idx]         = g_scene.scales[last_dense_idx];
			g_scene.modelIDs[deleted_dense_idx]       = g_scene.modelIDs[last_dense_idx];
			g_scene.poolHandles[deleted_dense_idx]    = g_scene.poolHandles[last_dense_idx];

			is_visible[deleted_dense_idx]             = is_visible[last_dense_idx];
			morph_weight_offsets[deleted_dense_idx]   = morph_weight_offsets[last_dense_idx];
			morph_weight_counts[deleted_dense_idx]    = morph_weight_counts[last_dense_idx];
			attach_parent_entity[deleted_dense_idx]   = attach_parent_entity[last_dense_idx];
			attach_parent_bone[deleted_dense_idx]     = attach_parent_bone[last_dense_idx];
			attach_offset_pos[deleted_dense_idx]      = attach_offset_pos[last_dense_idx];
			attach_offset_rot[deleted_dense_idx]      = attach_offset_rot[last_dense_idx];

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
		if (!g_registry.is_visible[i]) continue;

		uint32_t parent_idx = g_registry.attach_parent_entity[i];
		int32_t parent_bone = g_registry.attach_parent_bone[i];

		if (parent_idx != 0xFFFFFFFF && parent_bone >= 0) {
			uint32_t parent_pool_handle = g_scene.poolHandles[parent_idx];
			const auto* parent_pool = g_animPoolRegistry.GetPool(parent_pool_handle);

			glm::mat4 bone_model_mat(1.0f);

			if (parent_pool && parent_bone < static_cast<int32_t>(parent_pool->nodeCount)) {
				bone_model_mat = parent_pool->globalTransforms[parent_bone];
			}

			glm::mat4 offset_mat = glm::translate(glm::mat4(1.0f), g_registry.attach_offset_pos[i]) * glm::mat4_cast(g_registry.attach_offset_rot[i]);
			glm::mat4 final_attachment_mat = bone_model_mat * offset_mat;

			glm::vec3 parent_world_pos = g_scene.worldPositions[parent_idx];
			glm::quat parent_world_rot = g_scene.rotations[parent_idx];
			glm::vec3 parent_scale     = g_scene.scales[parent_idx];

			glm::mat4 parent_world_mat = glm::translate(glm::mat4(1.0f), parent_world_pos) * glm::mat4_cast(parent_world_rot) * glm::scale(glm::mat4(1.0f),
			                             parent_scale);
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


enum class ParseStep {
	parsing,
	baking,
	done,
	cancelled
};

struct ProgressUpdate {
	uint32_t requestID;
	ParseStep step;
	char message[128]; // HARDCODED
};

// queue just for telemetry might delete
inline LockFreeMPMC<ProgressUpdate, 128> g_progress_queue;

struct PoolMigrationReq {
	uint32_t poolHandle;
	ExecutionDomain targetDomain;
};
inline LockFreeMPMC<PoolMigrationReq, 256> g_poolMigrationQueue;

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
	if (!pool->baseSkeleton) return;

	const int32_t* parents = pool->baseSkeleton->parentIndices.data();
	const uint32_t* jointToNode = pool->baseSkeleton->jointToNodeMap.data();
	const glm::mat4* inverseBinds = pool->baseSkeleton->inverseBindMatrices.data();
	glm::mat4* outGlobals = pool->globalTransforms.data();

	uint32_t b_count = pool->nodeCount;
	uint32_t j_count = pool->jointCount;
	uint32_t offset = pool->globalJointOffset;

	for (uint32_t b = 0; b < b_count; ++b) {
		glm::mat4 localMat = glm::translate(glm::mat4(1.0f), locals[b].T) * glm::mat4_cast(locals[b].R) * glm::scale(glm::mat4(1.0f), locals[b].S);
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

template <uint8_t Flags>
void ProcessPoolChunk(const uint32_t* poolHandles, size_t count, float deltaTime, uint32_t write_frame) {
	ZoneScopedN("ProcessPoolChunk");

	for (size_t i = 0; i < count; ++i) {
		uint32_t poolHandle = poolHandles[i];
		AnimPoolData& pool = g_animPoolRegistry.pools[poolHandle];

		if (pool.frameCount == 0 || pool.duration <= 0.0f) continue;

		pool.currentTime += deltaTime * pool.playbackSpeed;
		float timePos = pool.currentTime;
		bool loop = pool.loop;

		float adjustedTime = loop ? std::fmod(timePos, pool.duration) : std::min(timePos, pool.duration);

		if (adjustedTime < 0.0f) adjustedTime += pool.duration;

		uint32_t frameIndex = static_cast<uint32_t>(adjustedTime * pool.sampleRate);

		if (frameIndex >= pool.frameCount) frameIndex = pool.frameCount - 1;

		if constexpr ((Flags & ANIM_FLAG_CPU_BAKED) != 0) {
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
			if (pool.retainCpuData) {

			}
		}

		if constexpr ((Flags & ANIM_FLAG_IK) != 0) {
			if (pool.retainCpuData) {

			}
		}

	}
}

inline void DispatchAnimationJobs(float deltaTime, uint32_t write_frame) {
	ZoneScoped;

	uint32_t active_pools = static_cast<uint32_t>(g_animPoolRegistry.pools.size());

	if (active_pools <= 1) return;

	ThreadLocalArena& arena = get_local_arena();
	arena.reset();

	constexpr uint8_t MAX_FLAGS = 64;
	uint32_t* buckets[MAX_FLAGS];
	uint32_t bucket_counts[MAX_FLAGS] = {0};

	for (uint8_t i = 0; i < MAX_FLAGS; ++i) {
		buckets[i] = static_cast<uint32_t*>(arena.allocate(active_pools * sizeof(uint32_t)));
	}

	bool has_active = false;

	for (uint32_t p = 1; p < AnimPoolRegistry::MAX_POOLS; ++p) {
		if (!g_animPoolRegistry.is_active[p]) continue;

		AnimPoolData& pool = g_animPoolRegistry.pools[p];

		if (pool.frameCount == 0) continue;

		has_active = true;
		uint8_t flagMask = static_cast<uint8_t>(pool.pipelineFlags);

		if (flagMask < MAX_FLAGS) {
			buckets[flagMask][bucket_counts[flagMask]++] = p;
		}
	}

	if (!has_active) return;

	g_anim_jobs_pending[write_frame].store(0, std::memory_order_relaxed);

	auto dispatchDomain = [&](uint8_t flag, auto func) {
		if (bucket_counts[flag] > 0) {
			g_anim_jobs_pending[write_frame].fetch_add(1, std::memory_order_relaxed);
			auto* bucket = buckets[flag];
			auto count = bucket_counts[flag];

			g_jobs.enqueue([bucket, count, deltaTime, write_frame, func]() {
				func(bucket, count, deltaTime, write_frame);
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
inline LockFreeMPMC<PoolPurgeReq, 256> g_poolPurgeQueue;

struct OrphanedCPUAnimData {
	std::vector<LocalTRS> locals;
	std::vector<glm::mat4> globals;
	std::vector<DualQuatScale> dqs;
};

inline void PurgePoolCPUDataAsync(AnimPoolData& pool) {
	auto* deadData = new OrphanedCPUAnimData{
		std::move(pool.localTracks),
		std::move(pool.globalTransforms),
		std::move(pool.finalJointDQs)
	};

	g_jobs.enqueue([deadData]() {
		delete deadData;
	});
}
}
