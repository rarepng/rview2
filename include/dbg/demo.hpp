#pragma once
#include <cstdint>
#include <array>
#include <span>
#include <glm/glm.hpp>
#include <core/scene.hpp>
#include <model_manager.hpp>

namespace rdemo {
    //maybe will move to somwhere central along with debug idk..
#ifdef RVIEW_DEMO
    inline constexpr bool is_active = true;
#else
    inline constexpr bool is_active = false;
#endif

    inline float pcg_hash(uint32_t input) {
        uint32_t state = input * 747796405u + 2891336453u;
        uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        uint32_t result = (word >> 22u) ^ word;
        return (float)result / 4294967295.0f;
    }

    struct DemoSpawn {
        const char* filepath;
        uint32_t count;
        glm::vec3 origin;
        void (*modifier)(uint32_t idx, uint32_t total, model_manager::Entity e, glm::vec3 origin);
    };

    struct DemoScene {
        const char* name;
        std::span<const DemoSpawn> spawns;
    };

    inline constexpr std::array<DemoSpawn, 2> MIXED_GRID_SPAWNS = {{
        {
            "resources/t1.glb", 200, glm::vec3(0.0f),
            [](uint32_t idx, uint32_t total, model_manager::Entity e, glm::vec3 origin) {
                int cols = 100;
                int row = idx / cols;
                int col = idx % cols;
                model_manager::g_registry.position(e) = origin + glm::vec3(row * 2.5f, 0.0f, col * 2.5f);

                uint32_t dense = model_manager::g_registry.get_dense_index(e);
                uint32_t modelID = g_scene.modelIDs[dense];
                
                auto it = model_manager::g_cpuModels.find(modelID);
                if (it != model_manager::g_cpuModels.end() && !it->second.bakedClips.empty()) {
                    uint32_t num_clips = it->second.bakedClips.size();
                    model_manager::g_registry.anim_clip[dense] = static_cast<int>(pcg_hash(idx) * num_clips);
                    model_manager::g_registry.anim_speed[dense] = 0.8f + (pcg_hash(idx * 1337) * 0.4f);
                }
            }
        },
        {
            "resources/t0.glb", 1, glm::vec3(1.25f, 0.0f, 1.25f),
            [](uint32_t idx, uint32_t total, model_manager::Entity e, glm::vec3 origin) {
                int cols = 100;
                int row = idx / cols;
                int col = idx % cols;
                
                model_manager::g_registry.position(e) = origin + glm::vec3(row * 2.5f, 0.0f, col * 2.5f);

                uint32_t dense = model_manager::g_registry.get_dense_index(e);
                uint32_t modelID = g_scene.modelIDs[dense];
                
                auto it = model_manager::g_cpuModels.find(modelID);
                if (it != model_manager::g_cpuModels.end() && !it->second.bakedClips.empty()) {
                    model_manager::g_registry.anim_clip[dense] = static_cast<int>(pcg_hash(idx + 5000) * it->second.bakedClips.size());
                    model_manager::g_registry.anim_speed[dense] = 0.8f + (pcg_hash(idx * 999) * 0.4f);
                }
            }
        }
    }};

    inline constexpr std::array<DemoScene, 1> SCENES = {{
        {"20k Mixed Grid Chaos", MIXED_GRID_SPAWNS}
    }};

}