#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <chrono>

#include <mutex>

#include <numeric>

#include "timer.hpp"
#include "ui.hpp"
#include "vkcam.hpp"


#include "playoutgeneric.hpp"


#include "vkobjs.hpp"





class vkrenderer {
public:
    vkrenderer(SDL_Window* wind,const SDL_DisplayMode* mode,bool* mshutdown,SDL_Event* e);
	bool init();
    void setsize(unsigned int w, unsigned int h);
    bool uploadfordraw();
    bool uploadfordraw(std::shared_ptr<playoutgeneric>& x);
    bool draw();
    bool drawpause();
	bool drawloading();
	bool drawblank();
    void cleanloading();
	void toggleshader();
    void cleanup();
    void handleclick();
	void handlemouse(double x, double y);
	bool initscene();
    void initshop();
	ui* getuihandle();
	void startmoving();
    void moveplayer();
	bool checkphp();


    void sdlevent(SDL_Event* e);

	glm::vec3 raymarch();


	void checkforanimupdates();

	void updateanims();

    void gametick();


    bool quicksetup();

    vkobjs& getvkobjs();

	bool newconnection{ false };

private:

	size_t dummytick{ 0 };

	std::mutex animmtx{};
	std::mutex updatemtx{};


	std::vector<double> enemyhps{};



	std::shared_mutex pausedmtx{};


	std::mutex getinstsettingsmtx{};


    vkobjs mvkobjs{};


	double dummy{ 0.0 };

	glm::vec<2,double> lastmousexy{};

	glm::vec2 movediff{};


	double decaystart{};
	bool inmenu{ true };


	unsigned int deathanddecaycd{ 120 };

	bool paused{ false };

	std::chrono::high_resolution_clock::time_point starttime = std::chrono::high_resolution_clock::now();
	std::chrono::high_resolution_clock::time_point starttick = std::chrono::high_resolution_clock::now();

	vkcam mcam{};

    std::vector<std::shared_ptr<playoutgeneric>> mplayer;
    std::vector<std::shared_ptr<playoutgeneric>> mplayerbuffer;
	
	selection selectiondata{};


	bool mmodeluploadrequired{ true };

	bool playermoving{ false };

	glm::vec3 playermoveto{ 0.0f };
    glm::vec3 playerlookto{ 0.0f };

	float tmpx{}, tmpy{};

	bool rdscene{ true };


	double pausebgntime{ 0.0 };
	double pausetime{ 0.0 };

	double lifetime{ 0.0 };
	double lifetime2{ 0.0 };

    double decaytime{ 0.0 };


	bool mlock{};
	int mousex{ 0 };
	int mousey{ 0 };
	double mlasttick{ 0.0 };
	void movecam();
	int mcamforward{ 0 };
	int mcamstrafe{ 0 };
	int mcamupdown{ 0 };
	



    std::vector<unsigned int> playercount{ 1 };
    std::vector<std::string> playerfname{ {"resources/p0.glb"} };
    const std::vector<std::vector<std::string>> playershaders{ {"shaders/player.vert.spv", "shaders/player.frag.spv"} };



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



	std::vector<glm::mat4> mpersviewmats{};

	bool switchshader{ false };



    bool setupplayer();
	bool setupmodels();
	bool setupmodels2();
	bool deviceinit();
	bool getqueue();
	bool createdepthbuffer();
	bool createswapchain();
	bool createrenderpass();
	bool setupstaticmodels();
	bool setupstaticmodels2();
	bool createframebuffer();
	bool createcommandpool();
	bool createcommandbuffer();
	bool createsyncobjects();
	bool initui();
	bool initgameui();

	bool initmenubackground();

	bool initvma();

	bool recreateswapchain();


	bool initsetupfinished{ false };

};
