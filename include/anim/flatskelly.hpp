#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstdint>

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