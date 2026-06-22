#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <core/jobs.hpp>
#include <cstring>

constexpr uint32_t MAX_BONES = 512;

struct alignas(64) FlatSkeleton {
	FlatSkeleton() {
		gltfToTopoMap.fill(-1);
		parentIndices.fill(-1);
	}
	uint32_t nodeCount{0};
	uint32_t jointCount{0};
	uint32_t pad[14];

	alignas(64) std::array<int32_t, MAX_BONES> parentIndices{};
	alignas(64) std::array<glm::mat4, MAX_BONES> localTransforms{};
	alignas(64) std::array<glm::mat4, MAX_BONES> globalTransforms{};

	alignas(64) std::array<uint32_t, MAX_BONES> jointToNodeMap{};
	alignas(64) std::array<glm::mat4, MAX_BONES> inverseBindMatrices{};

	alignas(64) std::array<glm::mat4, MAX_BONES> finalJointMatrices{};

	alignas(64) std::array<int32_t, MAX_BONES> gltfToTopoMap{};

	inline void UpdateGlobalMatrices(const glm::mat4& worldTransform = glm::mat4(1.0f)) {
		for (uint32_t i = 0; i < nodeCount; ++i) {
			int32_t parent = parentIndices[i];

			if (parent >= 0) {
				globalTransforms[i] = globalTransforms[parent] * localTransforms[i];
			} else {
				globalTransforms[i] = worldTransform * localTransforms[i];
			}
		}

		for (uint32_t j = 0; j < jointCount; ++j) {
			uint32_t nodeIdx = jointToNodeMap[j];
			finalJointMatrices[j] = globalTransforms[nodeIdx] * inverseBindMatrices[j];
		}
	}
};


struct ParsedSkeleton {
	uint32_t nodeCount{0};
	uint32_t jointCount{0};
	// idk if pointers here are the best idea but i cant think of how else to do it without breaking bss/stack
	std::unique_ptr<int32_t[]> parentIndices;
	std::unique_ptr<glm::mat4[]> localTransforms;
	std::unique_ptr<glm::mat4[]> globalTransforms;
	std::unique_ptr<uint32_t[]> jointToNodeMap;
	std::unique_ptr<glm::mat4[]> inverseBindMatrices;
	std::unique_ptr<glm::mat4[]> finalJointMatrices;
	std::unique_ptr<int32_t[]> gltfToTopoMap;

	ParsedSkeleton() = default;

	void allocate(uint32_t nodes, uint32_t joints, uint32_t gltfNodes) {
		nodeCount = nodes;
		jointCount = joints;

		if (nodes > 0) {
			parentIndices = std::make_unique<int32_t[]>(nodes);
			localTransforms = std::make_unique<glm::mat4[]>(nodes);
			globalTransforms = std::make_unique<glm::mat4[]>(nodes);

			for (uint32_t i = 0; i < nodes; ++i) {
				parentIndices[i] = -1;
				localTransforms[i] = glm::mat4(1.0f);
				globalTransforms[i] = glm::mat4(1.0f);
			}
		}

		if (joints > 0) {
			jointToNodeMap = std::make_unique<uint32_t[]>(joints);
			inverseBindMatrices = std::make_unique<glm::mat4[]>(joints);
			finalJointMatrices = std::make_unique<glm::mat4[]>(joints);

			for (uint32_t i = 0; i < joints; ++i) {
				jointToNodeMap[i] = 0;
				inverseBindMatrices[i] = glm::mat4(1.0f);
				finalJointMatrices[i] = glm::mat4(1.0f);
			}
		}

		if (gltfNodes > 0) {
			gltfToTopoMap = std::make_unique<int32_t[]>(gltfNodes);

			for (uint32_t i = 0; i < gltfNodes; ++i) gltfToTopoMap[i] = -1;
		}
	}

	inline void UpdateGlobalMatrices(const glm::mat4& worldTransform = glm::mat4(1.0f)) {
		if (nodeCount == 0) return;

		for (uint32_t i = 0; i < nodeCount; ++i) {
			int32_t parent = parentIndices[i];

			if (parent >= 0) {
				globalTransforms[i] = globalTransforms[parent] * localTransforms[i];
			} else {
				globalTransforms[i] = worldTransform * localTransforms[i];
			}
		}

		for (uint32_t j = 0; j < jointCount; ++j) {
			uint32_t nodeIdx = jointToNodeMap[j];
			finalJointMatrices[j] = globalTransforms[nodeIdx] * inverseBindMatrices[j];
		}
	}
};

struct JobSkeletonState {
	uint32_t nodeCount{0};
	uint32_t jointCount{0};

	int32_t* parentIndices = nullptr;
	glm::mat4* localTransforms = nullptr;
	glm::mat4* globalTransforms = nullptr;
	uint32_t* jointToNodeMap = nullptr;
	glm::mat4* inverseBindMatrices = nullptr;
	glm::mat4* finalJointMatrices = nullptr;

	void allocate_from_arena(ThreadLocalArena& arena, const ParsedSkeleton& source) {
		nodeCount = source.nodeCount;
		jointCount = source.jointCount;

		if (nodeCount > 0) {
			parentIndices = static_cast<int32_t*>(arena.allocate(nodeCount * sizeof(int32_t)));
			localTransforms = static_cast<glm::mat4*>(arena.allocate(nodeCount * sizeof(glm::mat4)));
			globalTransforms = static_cast<glm::mat4*>(arena.allocate(nodeCount * sizeof(glm::mat4)));

			std::memcpy(parentIndices, source.parentIndices.get(), nodeCount * sizeof(int32_t));
			std::memcpy(localTransforms, source.localTransforms.get(), nodeCount * sizeof(glm::mat4));
			std::memcpy(globalTransforms, source.globalTransforms.get(), nodeCount * sizeof(glm::mat4));
		}

		if (jointCount > 0) {
			jointToNodeMap = static_cast<uint32_t*>(arena.allocate(jointCount * sizeof(uint32_t)));
			inverseBindMatrices = static_cast<glm::mat4*>(arena.allocate(jointCount * sizeof(glm::mat4)));
			finalJointMatrices = static_cast<glm::mat4*>(arena.allocate(jointCount * sizeof(glm::mat4)));

			std::memcpy(jointToNodeMap, source.jointToNodeMap.get(), jointCount * sizeof(uint32_t));
			std::memcpy(inverseBindMatrices, source.inverseBindMatrices.get(), jointCount * sizeof(glm::mat4));
		}
	}

	inline void UpdateGlobalMatrices(const glm::mat4& worldTransform = glm::mat4(1.0f)) {
		if (nodeCount == 0) return;

		for (uint32_t i = 0; i < nodeCount; ++i) {
			int32_t parent = parentIndices[i];

			if (parent >= 0) {
				globalTransforms[i] = globalTransforms[parent] * localTransforms[i];
			} else {
				globalTransforms[i] = worldTransform * localTransforms[i];
			}
		}

		for (uint32_t j = 0; j < jointCount; ++j) {
			uint32_t nodeIdx = jointToNodeMap[j];
			finalJointMatrices[j] = globalTransforms[nodeIdx] * inverseBindMatrices[j];
		}
	}
};
