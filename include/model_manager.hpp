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
inline std::vector<int32_t>   g_bone_parents;
inline std::vector<uint32_t>  g_bone_entity_owner;
inline std::vector<glm::mat4> g_bone_locals;
inline std::vector<glm::mat4> g_bone_globals;
inline std::vector<uint32_t>  g_joint_to_node;
inline std::vector<glm::mat4> g_joint_inverse_binds;
inline std::vector<DualQuatScale> g_joint_final_matrices;
inline std::vector<GpuBuffer> g_gpu_joint_buffers;

constexpr uint32_t INVALID_ANIM_POOL = 0xFFFFFFFF;

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

struct AnimPoolData {
	uint64_t skeletonSignature = 0;
	uint32_t nodeCount = 0;
	uint32_t jointCount = 0;
	uint32_t frameCount = 0;

	float duration = 0.0f;
	float sampleRate = 30.0f;
	std::string name;

	std::vector<LocalTRS> localTracks;
	std::vector<glm::mat4> globalTransforms;

	std::vector<DualQuatScale> finalJointDQs;
};

void init_gpu_joint_buffers(rvkbucket& mvkobjs);


inline void reserve_mega_buffers(size_t capacity) {
	g_bone_parents.reserve(capacity);
	g_bone_entity_owner.reserve(capacity);
	g_bone_locals.reserve(capacity);
	g_bone_globals.reserve(capacity);
	g_joint_to_node.reserve(capacity);
	g_joint_inverse_binds.reserve(capacity);
	g_joint_final_matrices.reserve(capacity);
}

struct AnimPoolRegistry {
	std::mutex mtx;
	std::vector<AnimPoolData> pools;
	std::unordered_map<std::string, uint32_t> nameToHandle;

	uint32_t RegisterPool(AnimPoolData&& data) {
		std::lock_guard<std::mutex> lock(mtx);

		// Fast-path deduplication by name (massive fail HORRIBLE IDEA)
		// auto it = nameToHandle.find(data.name);
		// if (it != nameToHandle.end()) {
		// 	return it->second;
		// }

		uint32_t handle = static_cast<uint32_t>(pools.size());
		std::string unique_name = data.name + "_uid" + std::to_string(handle);
		nameToHandle[unique_name] = handle;
		pools.push_back(std::move(data));
		return handle;
	}

	const AnimPoolData* GetPool(uint32_t handle) const {
		if (handle < pools.size()) return &pools[handle];

		return nullptr;
	}
};

inline AnimPoolRegistry g_animPoolRegistry;

enum AnimPipelineFlags : uint8_t {
	ANIM_FLAG_NONE          = 0,
	ANIM_FLAG_GPU_BAKED     = 1 << 0,
	ANIM_FLAG_CPU_EVAL      = 1 << 1,
	ANIM_FLAG_BLENDING      = 1 << 2,
	ANIM_FLAG_IK            = 1 << 3,

	// demo masks
	MASK_HERO = ANIM_FLAG_CPU_EVAL | ANIM_FLAG_BLENDING | ANIM_FLAG_IK,
	MASK_HYBRID = ANIM_FLAG_CPU_EVAL | ANIM_FLAG_IK
};

inline AnimPipelineFlags operator|(AnimPipelineFlags a, AnimPipelineFlags b) {
	return static_cast<AnimPipelineFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline AnimPipelineFlags operator&(AnimPipelineFlags a, AnimPipelineFlags b) {
	return static_cast<AnimPipelineFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
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



void upload_joint_matrices(rvkbucket& mvkobjs);


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
	static constexpr uint32_t MAX_BLEND_LAYERS = 4;
	std::array<uint32_t, MAX_ENTITIES> dense_to_sparse;
	std::array<uint32_t, MAX_ENTITIES> generations;

	alignas(64) std::array<AnimPipelineFlags, MAX_ENTITIES> pipeline_flags{};
	alignas(64) std::array<uint32_t, MAX_ENTITIES> current_pool{};
	alignas(64) std::array<uint32_t, MAX_ENTITIES> target_pool{};

	alignas(64) std::array<float, MAX_ENTITIES> anim_speed{};
	alignas(64) std::array<bool, MAX_ENTITIES> anim_loop{};
	alignas(64) std::array<bool, MAX_ENTITIES> is_visible{};

	alignas(64) std::array<uint32_t, MAX_ENTITIES> ik_active_chains{};
	alignas(64) std::array<std::array<int, rview::anim::MAX_IK_CHAINS>, MAX_ENTITIES> ik_effector_node{};
	alignas(64) std::array<std::array<int, rview::anim::MAX_IK_CHAINS>, MAX_ENTITIES> ik_root_node{};
	alignas(64) std::array<std::array<glm::vec3, rview::anim::MAX_IK_CHAINS>, MAX_ENTITIES> ik_target_pos{};

	alignas(64) std::array<uint32_t, MAX_ENTITIES> bone_start_index{};
	alignas(64) std::array<uint32_t, MAX_ENTITIES> bone_count{};
	alignas(64) std::array<uint32_t, MAX_ENTITIES> joint_start_index{};
	alignas(64) std::array<uint32_t, MAX_ENTITIES> joint_count{};

	alignas(64) std::array<float, MAX_ENTITIES> blend_duration{};
	alignas(64) std::array<float, MAX_ENTITIES> current_blend_time{};
	alignas(64) std::array<std::vector<float>, MAX_ENTITIES> morph_weights{};
	alignas(64) std::array<float, MAX_ENTITIES> anim_magnitude{};
	alignas(64) std::array<std::array<int, MAX_BLEND_LAYERS>, MAX_ENTITIES> blend_clips{};
	alignas(64) std::array<std::array<float, MAX_BLEND_LAYERS>, MAX_ENTITIES> blend_weights{};

	alignas(64) std::array<uint32_t, MAX_ENTITIES> attach_parent_entity{};
	alignas(64) std::array<int32_t, MAX_ENTITIES> attach_parent_bone{};
	alignas(64) std::array<glm::vec3, MAX_ENTITIES> attach_offset_pos{};
	alignas(64) std::array<glm::quat, MAX_ENTITIES> attach_offset_rot{};

	Registry() {
		sparse.fill(0xFFFFFFFF);
		dense_to_sparse.fill(0xFFFFFFFF);
		generations.fill(0);
		anim_speed.fill(1.0f);
		anim_loop.fill(true);
		is_visible.fill(true);
		ik_active_chains.fill(0);
		attach_parent_entity.fill(0xFFFFFFFF);
		attach_parent_bone.fill(-1);
		// ik_effector_node.fill(-1); //todo
		// ik_root_node.fill(-1); //todo
		bone_start_index.fill(0xFFFFFFFF);
		bone_count.fill(0);
		joint_start_index.fill(0xFFFFFFFF);
		joint_count.fill(0);
		pipeline_flags.fill(ANIM_FLAG_GPU_BAKED);
		current_pool.fill(INVALID_ANIM_POOL);
		target_pool.fill(INVALID_ANIM_POOL);
		blend_duration.fill(0.0f);
		current_blend_time.fill(0.0f);

		for (auto& x : blend_clips)
			x.fill(-1);

		for (auto& x : blend_weights)
			x.fill(0.0f);

		anim_magnitude.fill(1.0f);
	}

	Entity create_entity(uint32_t modelID, bool isSkinned, uint32_t b_start, uint32_t b_count, uint32_t j_start, uint32_t j_count) {
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
		g_scene.animTimePositions[dense_idx] = 0.0f;

		if (isSkinned) {
			g_scene.jointOffsets[dense_idx] = j_start;
		} else {
			g_scene.jointOffsets[dense_idx] = 0;
		}

		g_scene.isSkinned[dense_idx] = isSkinned ? 1 : 0;

		pipeline_flags[dense_idx] = ANIM_FLAG_GPU_BAKED;
		current_pool[dense_idx]   = INVALID_ANIM_POOL;
		target_pool[dense_idx]    = INVALID_ANIM_POOL;
		anim_speed[dense_idx] = 1.0f;
		anim_loop[dense_idx] = true;
		is_visible[dense_idx] = true;

		bone_start_index[dense_idx]  = b_start;
		bone_count[dense_idx]        = b_count;
		joint_start_index[dense_idx] = j_start;
		joint_count[dense_idx]       = j_count;
		uint32_t target_count = 0;

		if (modelID != 0xFFFFFFFF && modelID < g_assets.models.size()) {
			const ModelMetadata& meta = g_assets.models[modelID];

			if (meta.primitiveCount > 0) {
				target_count = g_assets.primitives[meta.firstPrimitiveIndex].targetCount;
			}
		}

		morph_weights[dense_idx].assign(target_count, 0.0f);

		return ent;
	}

	bool is_valid(Entity e) const {
		return e.is_valid() &&
		       e.index() < MAX_ENTITIES &&
		       generations[e.index()] == e.generation() &&
		       sparse[e.index()] != 0xFFFFFFFF;
	}

	// swap and pop?
	void destroy_entity(Entity e) {
		std::lock_guard<std::mutex> lock(registry_mtx);

		if (!is_valid(e)) return;

		uint32_t deleted_dense_idx = sparse[e.index()];
		uint32_t last_dense_idx = g_scene.entity_count.load(std::memory_order_relaxed) - 1;

		if (deleted_dense_idx != last_dense_idx) {
			uint32_t last_entity_idx = dense_to_sparse[last_dense_idx];
			g_scene.worldPositions[deleted_dense_idx]    	= g_scene.worldPositions[last_dense_idx];
			g_scene.rotations[deleted_dense_idx]         	= g_scene.rotations[last_dense_idx];
			g_scene.scales[deleted_dense_idx]            	= g_scene.scales[last_dense_idx];
			g_scene.modelIDs[deleted_dense_idx]          	= g_scene.modelIDs[last_dense_idx];
			g_scene.animTimePositions[deleted_dense_idx] 	= g_scene.animTimePositions[last_dense_idx];
			g_scene.jointOffsets[deleted_dense_idx]      	= g_scene.jointOffsets[last_dense_idx];
			g_scene.isSkinned[deleted_dense_idx]         	= g_scene.isSkinned[last_dense_idx];
			pipeline_flags[deleted_dense_idx] 				= pipeline_flags[last_dense_idx];
			current_pool[deleted_dense_idx]   				= current_pool[last_dense_idx];
			target_pool[deleted_dense_idx]  				= target_pool[last_dense_idx];
			anim_speed[deleted_dense_idx]       			= anim_speed[last_dense_idx];
			anim_loop[deleted_dense_idx]        			= anim_loop[last_dense_idx];
			is_visible[deleted_dense_idx]       			= is_visible[last_dense_idx];
			bone_start_index[deleted_dense_idx]  			= bone_start_index[last_dense_idx];
			bone_count[deleted_dense_idx]        			= bone_count[last_dense_idx];
			joint_start_index[deleted_dense_idx] 			= joint_start_index[last_dense_idx];
			joint_count[deleted_dense_idx]       			= joint_count[last_dense_idx];
			sparse[last_entity_idx] 						= deleted_dense_idx;
			dense_to_sparse[deleted_dense_idx] 				= last_entity_idx;
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


inline glm::mat4 get_bone_matrix(uint32_t dense_idx, uint32_t target_bone_topo_idx) {
	uint32_t b_start = g_registry.bone_start_index[dense_idx];

	if (b_start != 0xFFFFFFFF) {
		return g_bone_globals[b_start + target_bone_topo_idx];
	}

	return glm::mat4(1.0f);
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

inline void ProcessDeferredStateChanges(uint64_t current_engine_frame) {
	ZoneScopedN("Process State Changes");

	StateChangeReq req;

	while (g_stateChangeQueue.pop(req)) {
		g_pendingStateChanges.push_back(req);
	}

	for (int i = static_cast<int>(g_pendingStateChanges.size()) - 1; i >= 0; --i) {
		if (current_engine_frame >= g_pendingStateChanges[i].targetExecutionFrame) {
			const auto& readyReq = g_pendingStateChanges[i];

			g_registry.current_pool[readyReq.dense_idx] = readyReq.targetPool;
			g_registry.pipeline_flags[readyReq.dense_idx] = readyReq.targetFlags;

			// future me problems
			if ((readyReq.targetFlags & ANIM_FLAG_CPU_EVAL) != 0) {
				// ditto
			}

			g_pendingStateChanges[i] = g_pendingStateChanges.back();
			g_pendingStateChanges.pop_back();
		}
	}
}
inline void EvaluateFK_And_CompressDQS(
    uint32_t b_count, uint32_t j_count, uint32_t b_start, uint32_t j_start,
    const LocalTRS* locals) {
	for (uint32_t b = 0; b < b_count; ++b) {
		glm::mat4 localMat = glm::translate(glm::mat4(1.0f), locals[b].T) * glm::mat4_cast(locals[b].R) * glm::scale(glm::mat4(1.0f), locals[b].S);

		int32_t parent = g_bone_parents[b_start + b];

		if (parent >= 0) g_bone_globals[b_start + b] = g_bone_globals[parent] * localMat;
		else             g_bone_globals[b_start + b] = localMat;
	}

	for (uint32_t j = 0; j < j_count; ++j) {
		uint32_t globalNodeIdx = g_joint_to_node[j_start + j];
		glm::mat4 finalMat = g_bone_globals[globalNodeIdx] * g_joint_inverse_binds[j_start + j];
		g_joint_final_matrices[j_start + j] = Mat4ToDualQuatScale(finalMat);
	}
}

template <uint8_t Flags>
void ProcessAnimationChunk(const uint32_t* entities, size_t count, float deltaTime) {
	ZoneScopedN("ProcessAnimationChunk");

	for (size_t i = 0; i < count; ++i) {
		uint32_t dense_idx = entities[i];

		uint32_t poolHandle = g_registry.current_pool[dense_idx];
		const AnimPoolData* pool = g_animPoolRegistry.GetPool(poolHandle);

		if (!pool || pool->frameCount == 0) continue;

		g_scene.animTimePositions[dense_idx] += deltaTime * g_registry.anim_speed[dense_idx];
		float timePos = g_scene.animTimePositions[dense_idx];
		bool loop = g_registry.anim_loop[dense_idx];

		if constexpr (Flags == ANIM_FLAG_GPU_BAKED) {
			uint32_t j_start = g_registry.joint_start_index[dense_idx];
			uint32_t j_count = g_registry.joint_count[dense_idx];

			float adjustedTime = loop ? std::fmod(timePos, pool->duration) : std::min(timePos, pool->duration);
			uint32_t frameIndex = static_cast<uint32_t>(adjustedTime * pool->sampleRate);

			if (frameIndex >= pool->frameCount) frameIndex = pool->frameCount - 1;

			const DualQuatScale* frameDQs = &pool->finalJointDQs[frameIndex * pool->jointCount];
			std::memcpy(&g_joint_final_matrices[j_start], frameDQs, j_count * sizeof(DualQuatScale));
			continue;
		}

		if constexpr ((Flags & ANIM_FLAG_CPU_EVAL) != 0) {
			uint32_t b_start = g_registry.bone_start_index[dense_idx];
			uint32_t b_count = g_registry.bone_count[dense_idx];
			uint32_t j_start = g_registry.joint_start_index[dense_idx];
			uint32_t j_count = g_registry.joint_count[dense_idx];

			float adjustedTime = loop ? std::fmod(timePos, pool->duration) : std::min(timePos, pool->duration);
			uint32_t frameIndex = static_cast<uint32_t>(adjustedTime * pool->sampleRate);

			if (frameIndex >= pool->frameCount) frameIndex = pool->frameCount - 1;

			const LocalTRS* baseTracks = &pool->localTracks[frameIndex * pool->nodeCount];
			LocalTRS locals[MAX_BONES];

			std::memcpy(locals, baseTracks, b_count * sizeof(LocalTRS));

			// TODO
			if constexpr ((Flags & ANIM_FLAG_BLENDING) != 0) {
				// ditto
			}

			// TODO
			if constexpr ((Flags & ANIM_FLAG_IK) != 0) {
				// ditto
			}

			EvaluateFK_And_CompressDQS(b_count, j_count, b_start, j_start, locals);
		}
	}
}

template <uint8_t Flags>
void DispatchChunk(const uint32_t* entities, size_t count, float deltaTime) {
	if (count == 0) return;

	// should i chunk more if count > 64?
	g_jobs.enqueue([ = ]() {
		ProcessAnimationChunk<Flags>(entities, count, deltaTime);
	});
}

inline void UpdateAttachments() {
	ZoneScopedN("Update Attachments");
	uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

	for (uint32_t i = 0; i < active_instances; ++i) {
		if (!g_registry.is_visible[i]) continue;

		uint32_t parent_idx = g_registry.attach_parent_entity[i];
		int32_t parent_bone = g_registry.attach_parent_bone[i];

		if (parent_idx != 0xFFFFFFFF && parent_bone >= 0) {
			glm::mat4 bone_local_mat = get_bone_matrix(parent_idx, parent_bone);
			glm::mat4 offset_mat = glm::translate(glm::mat4(1.0f), g_registry.attach_offset_pos[i]) * glm::mat4_cast(g_registry.attach_offset_rot[i]);
			glm::mat4 final_attachment_mat = bone_local_mat * offset_mat;

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

inline void DispatchAnimationJobs(float deltaTime) {
	ZoneScoped;

	uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

	if (active_instances == 0) return;

	std::atomic<int> pending_chunks{0};

	ThreadLocalArena& arena = get_local_arena();
	arena.reset();

	constexpr uint8_t MAX_FLAGS = 16;
	uint32_t* buckets[MAX_FLAGS];
	uint32_t bucket_counts[MAX_FLAGS] = {0};

	// pre compute size instead of pre allocate?
	for (uint8_t i = 0; i < MAX_FLAGS; ++i) {
		buckets[i] = static_cast<uint32_t*>(arena.allocate(active_instances * sizeof(uint32_t)));
	}

	for (uint32_t i = 0; i < active_instances; ++i) {
		if (!g_registry.is_visible[i] || g_scene.modelIDs[i] == 0xFFFFFFFF) continue;

		uint8_t flagMask = static_cast<uint8_t>(g_registry.pipeline_flags[i]);

		if (flagMask < MAX_FLAGS) {
			buckets[flagMask][bucket_counts[flagMask]++] = i;
		}
	}

	if (bucket_counts[ANIM_FLAG_GPU_BAKED] > 0) {
		DispatchChunk<ANIM_FLAG_GPU_BAKED>(buckets[ANIM_FLAG_GPU_BAKED], bucket_counts[ANIM_FLAG_GPU_BAKED], deltaTime);
	}

	if (bucket_counts[ANIM_FLAG_CPU_EVAL] > 0) {
		DispatchChunk<ANIM_FLAG_CPU_EVAL>(buckets[ANIM_FLAG_CPU_EVAL], bucket_counts[ANIM_FLAG_CPU_EVAL], deltaTime);
	}

	if (bucket_counts[MASK_HYBRID] > 0) {
		DispatchChunk<MASK_HYBRID>(buckets[MASK_HYBRID], bucket_counts[MASK_HYBRID], deltaTime);
	}

	if (bucket_counts[MASK_HERO] > 0) {
		DispatchChunk<MASK_HERO>(buckets[MASK_HERO], bucket_counts[MASK_HERO], deltaTime);
	}

	// TODO


	if (bucket_counts[ANIM_FLAG_GPU_BAKED] > 0) {
		pending_chunks.fetch_add(1, std::memory_order_relaxed);

		auto* bucket = buckets[ANIM_FLAG_GPU_BAKED];
		auto count = bucket_counts[ANIM_FLAG_GPU_BAKED];

		g_jobs.enqueue([&pending_chunks, bucket, count, deltaTime]() {
			ScopedJobGuard guard(pending_chunks);
			ProcessAnimationChunk<ANIM_FLAG_GPU_BAKED>(bucket, count, deltaTime);
		});
	}

	while (pending_chunks.load(std::memory_order_acquire) > 0) {
		if (!g_jobs.help_execute()) {
			std::this_thread::yield();
		}
	}

	UpdateAttachments();

}

}