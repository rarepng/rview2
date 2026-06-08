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
#include "playoutgeneric.hpp"
#include "core/rvk.hpp"
#include "core/jobs.hpp"
#include "core/scene.hpp"
#include <vkvbo.hpp>

enum InputKey : uint8_t {
	Key_W = 0, Key_S, Key_A, Key_D, Key_Q, Key_E,
	Key_RightClick,
	Input_Count
};

namespace vkrenderer {

// inline std::chrono::high_resolution_clock::time_point starttick = std::chrono::high_resolution_clock::now();

inline std::bitset<Input_Count> input_state;

inline std::vector<std::shared_ptr<playoutgeneric>> mplayer;
inline LockFreeMPMC<std::shared_ptr<playoutgeneric>, 32> pending_models;
inline selection selectiondata{};
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

void sync_assets_to_gpu(VkCommandBuffer cmd, rvkbucket& mvkobjs);
bool ges();
bool initcpuQs(rvkbucket& mvkobjs);
bool initglobalmats(rvkbucket& mvkobjs);
bool initglobalinstances(rvkbucket& mvkobjs);
bool initglobalindirect(rvkbucket& mvkobjs);
bool initglobalidrectUNINDEXED(rvkbucket& mvkobjs);
void update_dynamic_instances(rvkbucket& mvkobjs);

void immediate_submit(rvkbucket& mvkobjs, std::function<void(VkCommandBuffer cbuffer)>&& fn);
bool init(rvkbucket& mvkobjs);
void setsize(rvkbucket& mvkobjs, unsigned int w, unsigned int h);
bool uploadfordraw(rvkbucket& mvkobjs, VkCommandBuffer cbuffer);
bool uploadfordraw(rvkbucket& mvkobjs, std::shared_ptr<playoutgeneric>& x);
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
