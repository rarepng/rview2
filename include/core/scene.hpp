#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <atomic>
#include <cstdint>

struct alignas(64) SceneData {
    static constexpr uint32_t MAX_ENTITIES = 65536;

    alignas(64) std::array<glm::vec3, MAX_ENTITIES> worldPositions{};
    alignas(64) std::array<glm::quat, MAX_ENTITIES> rotations{};
    alignas(64) std::array<glm::vec3, MAX_ENTITIES> scales{};
    
    alignas(64) std::array<uint32_t, MAX_ENTITIES> modelIDs{};
    alignas(64) std::array<float, MAX_ENTITIES> animTimePositions{};

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
        modelIDs[id] = 0;
        return id;
    }
};

inline SceneData g_scene{};