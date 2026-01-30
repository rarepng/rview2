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

#include "timer.hpp"
#include "ui.hpp"
#include "vkcam.hpp"

#include "playoutgeneric.hpp"

#include "core/rvk.hpp"

enum InputKey : uint8_t {
	Key_W = 0, Key_S, Key_A, Key_D, Key_Q, Key_E,
	Key_RightClick,
	Input_Count
};

namespace vkrenderer {

// static std::chrono::high_resolution_clock::time_point starttick = std::chrono::high_resolution_clock::now();

static std::bitset<Input_Count> input_state;
static std::vector<std::future<void>> pending_loads;
static std::mutex load_mutex;

static vkcam mcam{};
static std::vector<std::shared_ptr<playoutgeneric>> mplayer;
static std::vector<std::shared_ptr<playoutgeneric>> mplayerbuffer;
static selection selectiondata{};
static glm::vec3 playermoveto{0.0f};
static glm::vec3 playerlookto{0.0f};


static bool mlock{false};
static int mousex{0};
static int mousey{0};
static double mlasttick{0.0};
static int mcamforward{0};
static int mcamstrafe{0};
static int mcamupdown{0};

static std::vector<unsigned int> playercount{2};
static std::vector<std::string> playerfname{{"resources/t0.glb"}};
static const std::vector<std::vector<std::string>> playershaders{{"shaders/vx.spv", "shaders/px.spv"}};

static timer mframetimer{};
static timer manimupdatetimer{};
static timer mmatupdatetimer{};
static timer miktimer{};
static timer muploadubossbotimer{};
static timer muigentimer{};
static timer muidrawtimer{};
static VkDeviceSize mminuniformbufferoffsetalignment = 0;
static std::vector<glm::mat4> persviewproj{glm::mat4{1.0f},glm::mat4{1.0f}};


bool ges();
bool initcpuQs(rvkbucket& mvkobjs);

void immediate_submit(rvkbucket& mvkobjs,std::function<void(VkCommandBuffer cbuffer)>&& fn);
bool init(rvkbucket& mvkobjs);
void setsize(rvkbucket& mvkobjs, unsigned int w, unsigned int h);
bool uploadfordraw(rvkbucket& mvkobjs,VkCommandBuffer cbuffer);
bool uploadfordraw(rvkbucket& mvkobjs,std::shared_ptr<playoutgeneric> &x);
bool draw(rvkbucket& mvkobjs);
void cleanup(rvkbucket& mvkobjs);
void handleclick();
void handlemouse(double x, double y);
bool initscene(rvkbucket& mvkobjs);
void moveplayer();
void sdlevent(rvkbucket& mvkobjs,const SDL_Event& e);
void checkforanimupdates(rvkbucket& mvkobjs);
void updateanims(rvkbucket& mvkobjs);

bool createpools(rvkbucket& mvkobjs);
bool deviceinit();
bool getqueue(rvkbucket& mvkobjs);
bool createdepthbuffer(rvkbucket& mvkobjs);
bool createswapchain(rvkbucket& mvkobjs);
bool createrenderpass(rvkbucket& mvkobjs);
bool createframebuffer(rvkbucket& mvkobjs);
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
