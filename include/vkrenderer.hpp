#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <SDL3/SDL.h>
#include <VkBootstrap.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <bitset>
#include <mutex>
#include <future>
#include <numeric>
#include <simdjson.h>
#include "timer.hpp"
#include "ui.hpp"
#include "vkcam.hpp"
#include "core/rvk.hpp"
#include "core/jobs.hpp"
#include "core/scene.hpp"
#include <vkvbo.hpp>
#include "model_manager.hpp"

enum InputKey : uint8_t {
	Key_W = 0, Key_S, Key_A, Key_D, Key_Q, Key_E,
	Key_RightClick,
	Input_Count
};
namespace rview {
namespace io {
inline void save_state_to_json() {
	std::lock_guard<std::mutex> lock(model_manager::g_modelResourceMtx);
	std::ofstream out("state.json");

	if (!out.is_open()) {
		std::cerr << "Failed to open state.json for writing.\n";
		return;
	}
	out << "{\n  \"models\": [\n";
	std::map<uint32_t, std::vector<glm::vec3>> model_instances;
	uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);
	for (uint32_t i = 0; i < active_instances; ++i) {
		uint32_t modelID = g_scene.modelIDs[i];

		if (modelID == 0xFFFFFFFF) continue;

		model_instances[modelID].push_back(g_scene.worldPositions[i]);
	}
	bool first_model = true;
	for (auto const& [modelID, positions] : model_instances) {
		if (model_manager::g_model_filepaths.find(modelID) == model_manager::g_model_filepaths.end()) continue;

		if (!first_model) out << ",\n";

		first_model = false;

		const std::string& filepath = model_manager::g_model_filepaths[modelID];

		out << "    {\n";
		out << "      \"file\": \"" << filepath << "\",\n";
		out << "      \"count\": " << positions.size() << ",\n";
		out << "      \"positions\": [\n";
		for (size_t p = 0; p < positions.size(); ++p) {
			out << "        [" << positions[p].x << ", " << positions[p].y << ", " << positions[p].z << "]";

			if (p < positions.size() - 1) out << ",";

			out << "\n";
		}
		out << "      ],\n";
		out << "      \"shaders\": {\n";
		out << "        \"vx\": \"shaders/vx.spv\",\n";
		out << "        \"px\": \"shaders/px.spv\",\n";
		out << "        \"cx\": \"shaders/cx.spv\"\n";
		out << "      }\n";
		out << "    }";
	}
	out << "\n  ]\n}\n";
}
};
};

namespace vkrenderer {

// inline std::chrono::high_resolution_clock::time_point starttick = std::chrono::high_resolution_clock::now();

inline std::bitset<Input_Count> input_state;

inline LockFreeMPMC<model_manager::StagingModelData, 32> pending_staging_models;
inline glm::vec3 playermoveto{0.0f};
inline glm::vec3 playerlookto{0.0f};


inline int mousex{0};
inline int mousey{0};
inline double mlasttick{0.0};

inline timer mframetimer{};
inline timer manimupdatetimer{};
inline timer mmatupdatetimer{};
inline timer miktimer{};
inline timer muploadubossbotimer{};
inline timer muigentimer{};
inline timer muidrawtimer{};
inline std::vector<glm::mat4> persviewproj{glm::mat4{1.0f}, glm::mat4{1.0f}};

// TODOOOOOOO
inline VkBuffer m_rawIndexSSBO = VK_NULL_HANDLE;
inline VmaAllocation m_rawIndexAlloc = VK_NULL_HANDLE;
inline uint32_t m_rawIndexCapacity = 0;
inline uint32_t m_rawIndexOffset = 0;
inline VkBuffer m_primitiveRegistrySSBO = VK_NULL_HANDLE;
inline VmaAllocation m_primitiveRegistryAlloc = VK_NULL_HANDLE;
inline VkBuffer m_modelRegistrySSBO = VK_NULL_HANDLE;
inline VmaAllocation m_modelRegistryAlloc = VK_NULL_HANDLE;
inline std::atomic<bool> m_requiresUpload{false};

inline std::once_flag obswrite{};
inline std::once_flag spoutsend{};

bool initglobalmorphs(rvkbucket& mvkobjs);

void sync_assets_to_gpu(VkCommandBuffer cmd, rvkbucket& mvkobjs);
bool ges();
bool initcpuQs(rvkbucket& mvkobjs);
bool initglobalmats(rvkbucket& mvkobjs);
bool initglobalinstances(rvkbucket& mvkobjs);
bool initglobalindirect(rvkbucket& mvkobjs);
bool initglobalidrectUNINDEXED(rvkbucket& mvkobjs);
void update_dynamic_instances(rvkbucket& mvkobjs);
bool create_obs_target(rvkbucket& mvkobjs);
bool init_obs_bridge(void* shared_handle, uint32_t width, uint32_t height, const uint8_t* device_luid);
// void cleanup_obs_bridge();
void write_obs(rvkbucket& mvkobjs);
void copy_engine_to_obs(VkCommandBuffer cmdBuffer, rvkbucket& mvkobjs, VkImage engine_source_img, VkImage obs_target_img, uint32_t width,
                        uint32_t height);

void immediate_submit(rvkbucket& mvkobjs, std::function<void(VkCommandBuffer cbuffer)>&& fn);
bool init(rvkbucket& mvkobjs);
void setsize(rvkbucket& mvkobjs, unsigned int w, unsigned int h);
bool uploadfordraw(rvkbucket& mvkobjs, VkCommandBuffer cbuffer);
bool draw(rvkbucket& mvkobjs);
void cleanup(rvkbucket& mvkobjs);
void handleclick();
void handlemouse(double x, double y);
bool initscene(rvkbucket& mvkobjs);
void moveplayer();
void sdlevent(rvkbucket& mvkobjs, const SDL_Event& e);
void checkforanimupdates(rvkbucket& mvkobjs);
void updateanims(rvkbucket& mvkobjs);

bool deviceinit();
bool getqueue(rvkbucket& mvkobjs);
bool createdepthbuffer(rvkbucket& mvkobjs);
bool createswapchain(rvkbucket& mvkobjs);
bool createcommandpool(rvkbucket& mvkobjs);
bool createcommandbuffer(rvkbucket& mvkobjs);
bool createsyncobjects(rvkbucket& mvkobjs);
bool initui(rvkbucket& mvkobjs);
void movecam(rvkbucket& mvkobjs);
bool initvma(rvkbucket& mvkobjs);
bool recreateswapchain(rvkbucket& mvkobjs);
float navmesh(float x, float z);
glm::vec3 navmeshnormal(float x, float z);

};
