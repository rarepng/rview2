#pragma once
#include <cstdint>
#include <array>
#include <span>
#include <glm/glm.hpp>
#include <core/scene.hpp>
#include <model_manager.hpp>

namespace rdemo {

inline constexpr uint32_t calc_grid_dim(uint32_t total_instances) {
	uint32_t dim = 1;

	while (dim * dim * dim < total_instances) {
		dim++;
	}

	return dim;
}

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

				if (it != model_manager::g_cpuModels.end() && !it->second.defaultClips.empty()) {
					uint32_t num_clips = static_cast<uint32_t>(it->second.defaultClips.size());
					uint32_t rand_clip_idx = static_cast<uint32_t>(pcg_hash(idx) * num_clips);

					g_scene.poolHandles[dense] = it->second.defaultClips[rand_clip_idx];
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

				if (it != model_manager::g_cpuModels.end() && !it->second.defaultClips.empty()) {
					uint32_t num_clips = static_cast<uint32_t>(it->second.defaultClips.size());
					uint32_t rand_clip_idx = static_cast<uint32_t>(pcg_hash(idx) * num_clips);

					g_scene.poolHandles[dense] = it->second.defaultClips[rand_clip_idx];
				}
			}
		}
	}
};

inline constexpr std::array<DemoSpawn, 2> CUBE_GRID_SPAWNS = {{
		{
			"resources/ren.glb", 200, glm::vec3(0.0f),
			[](uint32_t idx, uint32_t total, model_manager::Entity e, glm::vec3 origin) {
				uint32_t dim = calc_grid_dim(total);

				uint32_t z = idx / (dim * dim);
				uint32_t y = (idx / dim) % dim;
				uint32_t x = idx % dim;

				float spacing = 3.0f;
				model_manager::g_registry.position(e) = origin + glm::vec3(x * spacing, y * spacing, z * spacing);

				uint32_t dense = model_manager::g_registry.get_dense_index(e);
				uint32_t modelID = g_scene.modelIDs[dense];

				auto it = model_manager::g_cpuModels.find(modelID);

				if (it != model_manager::g_cpuModels.end() && !it->second.defaultClips.empty()) {
					uint32_t num_clips = static_cast<uint32_t>(it->second.defaultClips.size());
					uint32_t rand_clip_idx = static_cast<uint32_t>(pcg_hash(idx) * num_clips);

					g_scene.poolHandles[dense] = it->second.defaultClips[rand_clip_idx];
				}
			}
		},
		{
			"resources/nelf.glb", 200, glm::vec3(1.5f, 1.5f, 1.5f),
			[](uint32_t idx, uint32_t total, model_manager::Entity e, glm::vec3 origin) {
				uint32_t dim = calc_grid_dim(total);

				uint32_t z = idx / (dim * dim);
				uint32_t y = (idx / dim) % dim;
				uint32_t x = idx % dim;

				float spacing = 3.0f;
				model_manager::g_registry.position(e) = origin + glm::vec3(x * spacing, y * spacing, z * spacing);

				uint32_t dense = model_manager::g_registry.get_dense_index(e);
				uint32_t modelID = g_scene.modelIDs[dense];

				auto it = model_manager::g_cpuModels.find(modelID);

				if (it != model_manager::g_cpuModels.end() && !it->second.defaultClips.empty()) {
					uint32_t num_clips = static_cast<uint32_t>(it->second.defaultClips.size());
					uint32_t rand_clip_idx = static_cast<uint32_t>(pcg_hash(idx) * num_clips);

					g_scene.poolHandles[dense] = it->second.defaultClips[rand_clip_idx];
				}
			}
		}
	}
};

inline constexpr std::array<DemoScene, 2> SCENES = {{
		{"20k mixed grid", MIXED_GRID_SPAWNS},
		{"20k interleaved", CUBE_GRID_SPAWNS}
	}
};

}