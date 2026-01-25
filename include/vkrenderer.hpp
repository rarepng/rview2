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

class vkrenderer {
public:
	vkrenderer(SDL_Window *wind, const SDL_DisplayMode *mode, bool *mshutdown, SDL_Event *e);

	void immediate_submit(std::function<void(VkCommandBuffer cbuffer)>&& fn);
	bool init();
	void setsize(unsigned int w, unsigned int h);
	bool uploadfordraw(VkCommandBuffer cbuffer);
	bool uploadfordraw(std::shared_ptr<playoutgeneric> &x);
	bool draw();
	bool drawloading();
	bool drawblank();
	void cleanup();
	void handleclick();
	void handlemouse(double x, double y);
	bool initscene();
	ui *getuihandle();
	void moveplayer();


	void sdlevent(const SDL_Event& e);


	void checkforanimupdates();

	void updateanims();


	bool newconnection{false};
	bool ges();
private:
	std::bitset<Input_Count> input_state;
	std::vector<std::future<void>> pending_loads;
	std::mutex load_mutex;
	std::function<void(rvkbucket::DummyTexture&)> destroyDummy =
	[this](rvkbucket::DummyTexture& tex) {
		vkDestroyImageView(mvkobjs.vkdevice.device, tex.view, nullptr);
		vkDestroyImage(mvkobjs.vkdevice.device, tex.image, nullptr);
		vkFreeMemory(mvkobjs.vkdevice.device, tex.memory, nullptr);
	};

	rvkbucket mvkobjs{};

	glm::vec2 movediff{};

	std::chrono::high_resolution_clock::time_point starttick = std::chrono::high_resolution_clock::now();

	vkcam mcam{};

	std::vector<std::shared_ptr<playoutgeneric>> mplayer;
	std::vector<std::shared_ptr<playoutgeneric>> mplayerbuffer;

	selection selectiondata{};

	bool mmodeluploadrequired{true};

	glm::vec3 playermoveto{0.0f};
	glm::vec3 playerlookto{0.0f};

	float tmpx{}, tmpy{};

	bool rdscene{true};

	double pausebgntime{0.0};
	double pausetime{0.0};

	double lifetime{0.0};
	double lifetime2{0.0};


	bool mlock{false};
	int mousex{0};
	int mousey{0};
	double mlasttick{0.0};
	void movecam();
	int mcamforward{0};
	int mcamstrafe{0};
	int mcamupdown{0};

	std::vector<unsigned int> playercount{1};
	std::vector<std::string> playerfname{{"resources/t0.glb"}};
	const std::vector<std::vector<std::string>> playershaders{{"shaders/vx.spv", "shaders/px.spv"}};

	ui mui{};
	timer mframetimer{};
	timer manimupdatetimer{};
	timer mmatupdatetimer{};
	timer miktimer{};
	timer muploadubossbotimer{};
	timer muigentimer{};
	timer muidrawtimer{};

	VkSurfaceKHR msurface{};

	VkDeviceSize mminuniformbufferoffsetalignment = 0;

	std::vector<glm::mat4> persviewproj{glm::mat4{1.0f},glm::mat4{1.0f}};

	bool createpools();
	bool deviceinit();
	bool getqueue();
	bool createdepthbuffer();
	bool createswapchain();
	bool createrenderpass();
	bool createframebuffer();
	bool createcommandpool();
	bool createcommandbuffer();
	bool createsyncobjects();
	bool initui();

	float navmesh(float x, float z);
	glm::vec3 navmeshnormal(float x, float z);

	bool initvma();

	bool recreateswapchain();

	bool initsetupfinished{false};
};
