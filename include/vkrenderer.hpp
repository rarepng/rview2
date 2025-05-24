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

#include <mutex>

#include <numeric>

#include "timer.hpp"
#include "ui.hpp"
#include "vkcam.hpp"

#include "playoutgeneric.hpp"

#include "vkobjs.hpp"

class vkrenderer {
public:
	vkrenderer(SDL_Window *wind, const SDL_DisplayMode *mode, bool *mshutdown, SDL_Event *e);
	bool init();
	void setsize(unsigned int w, unsigned int h);
	bool uploadfordraw();
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

	void sdlevent(SDL_Event *e);


	void checkforanimupdates();

	void updateanims();

	bool quicksetup();

	vkobjs &getvkobjs();

	bool newconnection{false};

private:
	size_t dummytick{0};

	std::mutex animmtx{};
	std::mutex updatemtx{};

	std::vector<double> enemyhps{};

	std::shared_mutex pausedmtx{};

	std::mutex getinstsettingsmtx{};

	vkobjs mvkobjs{};

	double dummy{0.0};

	glm::vec<2, double> lastmousexy{};

	glm::vec2 movediff{};

	bool inmenu{true};

	bool paused{false};

	std::chrono::high_resolution_clock::time_point starttime = std::chrono::high_resolution_clock::now();
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


	bool mlock{};
	int mousex{0};
	int mousey{0};
	double mlasttick{0.0};
	void movecam();
	int mcamforward{0};
	int mcamstrafe{0};
	int mcamupdown{0};

	std::vector<unsigned int> playercount{1};
	std::vector<std::string> playerfname{{"resources/p0.glb"}};
	const std::vector<std::vector<std::string>> playershaders{{"shaders/gen.slang.vx.spv", "shaders/gen.slang.px.spv"}};

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

	std::vector<glm::mat4> mpersviewmats{glm::mat4{1.0f},glm::mat4{1.0f}};

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

	bool initvma();

	bool recreateswapchain();

	bool initsetupfinished{false};
};
