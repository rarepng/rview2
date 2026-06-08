#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

struct alignas(64) SceneData {
	static constexpr uint32_t MAX_ENTITIES = 65536;

	alignas(64) std::array<glm::vec3, MAX_ENTITIES> worldPositions{};
	alignas(64) std::array<glm::quat, MAX_ENTITIES> rotations{};
	alignas(64) std::array<glm::vec3, MAX_ENTITIES> scales{};

	alignas(64) std::array<uint32_t, MAX_ENTITIES> modelIDs{};
	alignas(64) std::array<float, MAX_ENTITIES> animTimePositions{};
	alignas(64) std::array<uint32_t, MAX_ENTITIES> jointOffsets{};
	alignas(64) std::array<uint32_t, MAX_ENTITIES> isSkinned{};

	alignas(64) std::atomic<uint32_t> entity_count{0};

	inline uint32_t create_entity() {
		uint32_t id = entity_count.fetch_add(1, std::memory_order_relaxed);

		if (id >= MAX_ENTITIES) {
			entity_count.fetch_sub(1, std::memory_order_relaxed);
			return MAX_ENTITIES - 1;
		}

		worldPositions[id] = glm::vec3(0.0f);
		scales[id] = glm::vec3(1.0f);
		rotations[id] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		animTimePositions[id] = 0.0f;
		modelIDs[id] = 0xFFFFFFFF;
		return id;
	}
};
struct PrimitiveMetadata {
	uint32_t indexCount = 0;
	uint32_t indexByteOffset = 0;
	uint32_t indexType = 0;
	uint32_t posIdx = 0xFFFFFFFF;
	uint32_t norIdx = 0xFFFFFFFF;
	uint32_t tanIdx = 0xFFFFFFFF;
	uint32_t texIdx = 0xFFFFFFFF;
	uint32_t joiIdx = 0xFFFFFFFF;
	uint32_t weiIdx = 0xFFFFFFFF;
	uint32_t materialID = 0;
	uint32_t jointFmt = 0;
	uint32_t weightFmt = 2;
};

struct ModelMetadata {
	uint32_t firstPrimitiveIndex;
	uint32_t primitiveCount;
};

// The Global Asset Database (Static Data)
struct AssetRegistry {
	std::vector<uint8_t> globalRawIndices;
	std::vector<PrimitiveMetadata> primitives;
	std::vector<ModelMetadata> models;
	std::mutex registryMutex;
	std::atomic<bool> requiresUpload{false};

	uint32_t register_model(uint32_t primCount) {
		std::lock_guard<std::mutex> lock(registryMutex);
		uint32_t modelID = static_cast<uint32_t>(models.size());
		models.push_back({ static_cast<uint32_t>(primitives.size()), primCount });
		return modelID;
	}
};

inline uint32_t align_up_4(uint32_t size) {
	return (size + 3) & ~3;
}

inline AssetRegistry g_assets;
inline SceneData g_scene{};