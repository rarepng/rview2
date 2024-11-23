#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vk/VkBootstrap.h>
#include <vk/vk_mem_alloc.h>
#include <chrono>

#include <future>
#include <thread>
#include <mutex>


#include "timer.hpp"
#include "renderpass.hpp"
#include "playout.hpp"
#include "framebuffer.hpp"
#include "commandpool.hpp"
#include "commandbuffer.hpp"
#include "vktexture.hpp"
#include "ubo.hpp"
#include "ssbomesh.hpp"
#include "ssbo.hpp"
#include "vkvbo.hpp"
#include "vkebo.hpp"
#include "ui.hpp"
#include "vkcam.hpp"

#include "gamestate.hpp"

#include "playoutback.hpp"
#include "playoutplayer.hpp"
#include "playoutmodel.hpp"
#include "playoutstatic.hpp"
#include "playoutmenubg.hpp"
#include "playoutground.hpp"
#include "playoutcircle.hpp"


#include "vkobjs.hpp"

#include "vksyncobjects.hpp"




class vkrenderer {
public:
	vkrenderer(GLFWwindow* wind,GLFWmonitor* mont,const GLFWvidmode* mode);
	bool init();
	void setsize(unsigned int w, unsigned int h);
	bool uploadfordraw();
	void uploadforshop();
	bool draw();
	void drawshop();
	bool drawpause();
	bool drawmainmenu();
	bool drawloading();
	bool drawblank();
	void cleanloading();
	void cleanmainmenu();
	void toggleshader();
	void cleanup();
	void handlekey(int key, int scancode, int action, int mods);
	void handlekeymenu(int key, int scancode, int action, int mods);
	void handleclick(int key, int action, int mods);
	void handlemouse(double x, double y);
	bool initscene();
	void initshop();
	bool getserverclientstatus();
	ui* getuihandle();
	void startmoving();
	void moveplayer();
	void moveenemies();
	bool checkphp();

	bool intersercthitbox(glm::vec3 x,std::pair<glm::vec3,unsigned int> y);

	void dmgenemies();

	void checkenemies();

	glm::vec3 raymarch();



	void animateshop();


	void gametick();

	bool checkcooldown(unsigned int x);


	bool quicksetup(netclient* nclient);
	bool quicksetup(netserver* nserver);

	vkobjs& getvkobjs();
	netobjs& getnetobjs();


private:


	std::vector<double> enemyhps{};


	//gamestate mgamestate{};


	std::shared_mutex pausedmtx{};


	std::mutex getinstsettingsmtx{};


	vkobjs mvkobjs{};
	netobjs mnobjs{};
	gobjs mgobjs{};

	double* playerhp{ nullptr };

	double dummy{ 0.0 };

	glm::vec<2,double> lastmousexy{};

	glm::vec2 movediff{};

	std::vector<std::shared_ptr<spell>> mspells{ std::make_shared<spell>(0,false,true,false,120,20,0,0,500,glm::vec3{0.0f,0.0f,0.0f},0.22),std::make_shared<spell>(1,false,true,false,120,20,0,0,0,glm::vec3{0.0f,0.0f,0.0f},0.0) };

	double decaystart{};
	bool inmenu{ true };


	unsigned int deathanddecaycd{ 120 };

	bool paused{ false };

	std::chrono::high_resolution_clock::time_point starttime = std::chrono::high_resolution_clock::now();
	std::chrono::high_resolution_clock::time_point starttick = std::chrono::high_resolution_clock::now();

	vkcam mcam{};

	std::shared_ptr<playoutmenubg> mmenubg;
	std::shared_ptr<playoutmenubg> lifebar;

	std::shared_ptr<playoutback> mbackground;

	std::shared_ptr<playoutplayer> mplayer;
	std::shared_ptr<playoutground> mground;
	std::shared_ptr<playoutcircle> mcircle;
	std::shared_ptr<playoutcircle> mplates;


	std::vector<std::shared_ptr<playoutcircle>> mchoices;
	std::vector<std::shared_ptr<playoutstatic>> mstaticchoices;



	std::vector<std::shared_ptr<playoutmodel>> mpgltf;

	std::vector<std::shared_ptr<playoutstatic>> mstatic0;

	bool mmodeluploadrequired{ true };

	bool playermoving{ false };

	glm::vec3 playermoveto{ 0.0f };
	glm::vec3 playerlookto{ 0.0f };
	glm::vec3* playerlocation=nullptr;

	glm::vec3 decaypos{ 0.0f };
	//glm::vec3 deathanddecaypos{ 0.0f };

	float tmpx{}, tmpy{};

	bool rdscene{ true };


	double pausebgntime{ 0.0 };
	double pausetime{ 0.0 };

	double lifetime{ 0.0 };
	double lifetime2{ 0.0 };

	double decaytime{ 0.0 };
	double deathanddecaystart{ 0.0 };


	bool mlock{};
	int mousex{ 0 };
	int mousey{ 0 };
	double mlasttick{ 0.0 };
	void movecam();
	int mcamforward{ 0 };
	int mcamstrafe{ 0 };
	int mcamupdown{ 0 };
	

	double shopitemcount{ 2.0 };


	unsigned int dndcount{ 1 };
	unsigned int playercount{ 1 };
	unsigned int backgobjs{ 1 };
	unsigned int groundobjs{ 200000 };
	const std::vector<unsigned int> shopcounts{ 60,1 };
	const std::vector<unsigned int> animcounts{ 1,8 };
	const unsigned int totalanimcounts{ 9 };
	const std::vector<unsigned int> staticcounts{ };
	const std::string dndname{ "resources/dnd.png" };
	const std::string playerfname{ "resources/player.glb" };
	//const std::string playerfname{ "resources/tauren.glb" };
	const std::string backfname{ "resources/dontuse3.glb" };
	const std::string groundfname{ "resources/dontuse4.glb" };
	const std::vector<std::string> animfname{ "resources/untitled.glb","resources/untitled1.glb" };
	const std::vector<std::string> staticfname{ };
	const std::vector<std::string> staticshopfname{ "resources/dontuse2.glb","resources/dontuse1.glb" };
	const std::vector<std::string> dndshaders{ "shaders/dnd.vert.spv", "shaders/dnd.frag.spv" };
	const std::vector<std::string> plateshaders{ "shaders/plate.vert.spv", "shaders/plate.frag.spv" };
	const std::vector<std::string> menubgshaders{ "shaders/menufog.vert.spv", "shaders/menufog.frag.spv" };
	const std::vector<std::string> lifebarshaders{ "shaders/heart.vert.spv", "shaders/heart.frag.spv" };
	const std::vector<std::string> playershaders{ "shaders/player.vert.spv", "shaders/player.frag.spv" };
	const std::vector<std::string> backshaders{ "shaders/static.vert.spv", "shaders/static.frag.spv" };
	const std::vector<std::string> animshaders{ "shaders/gltf_gpu.vert.spv", "shaders/gltf_gpu.frag.spv" };
	const std::vector<std::string> staticshaders{ "shaders/static.vert.spv", "shaders/static.frag.spv" };
	const std::vector<std::string> groundshaders{ "shaders/ground.vert.spv", "shaders/ground.frag.spv" };
	const std::vector<std::string> shopbgshaders{ "shaders/shopbg.vert.spv", "shaders/shopbg.frag.spv" };
	const std::vector<std::string> staticshopshaders{ "shaders/staticshop.vert.spv", "shaders/staticshop.frag.spv" };


	const std::string circletexture{ "resources/dnd.png" };
	const std::string hptexture{ "resources/compliment.png" };


	const std::vector<std::string> shopbacktextures{ "resources/compliment.png" };

	ui mui{};
	timer mframetimer{};
	timer mmatgentimer{};
	timer miktimer{};
	timer muploadtovbotimer{};
	timer muploadtoubotimer{};
	timer muigentimer{};
	timer muidrawtimer{};

	VkSurfaceKHR msurface = VK_NULL_HANDLE;

	VkDeviceSize mminuniformbufferoffsetalignment = 0;



	std::vector<glm::mat4> mpersviewmats{};

	bool switchshader{ false };


	void wavesetup();


	bool setuplifebar();
	bool setuplifebar2();
	bool loadbackground();
	bool setupground();
	bool setupground2();
	bool setupplayer();
	bool setupplayer2();
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




};
