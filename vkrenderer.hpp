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

#include "timer.hpp"
#include "renderpass.hpp"
#include "gltfgpupipeline.hpp"
#include "playout.hpp"
#include "framebuffer.hpp"
#include "commandpool.hpp"
#include "commandbuffer.hpp"
#include "vksyncobjects.hpp"
#include "vktexture.hpp"
#include "ubo.hpp"
#include "ssbomesh.hpp"
#include "ssbo.hpp"
#include "vkvbo.hpp"
#include "indexbuffer.hpp"
#include "ui.hpp"
#include "vkcam.hpp"

#include "vkgltfstatic.hpp"
#include "staticinstance.hpp"

#include "playoutplayer.hpp"
#include "playoutmodel.hpp"
#include "playoutstatic.hpp"

#include "vkgltfmodel.hpp"
#include "vkgltfinstance.hpp"

//#include "coord.hpp"

#include "vkobjs.hpp"






class vkrenderer {
public:
	vkrenderer(GLFWwindow* wind,GLFWmonitor* mont,const GLFWvidmode* mode);
	bool init();
	void setsize(unsigned int w, unsigned int h);
	bool draw();
	bool drawmainmenu();
	bool drawloading();
	void toggleshader();
	void cleanup();
	void handlekey(int key, int scancode, int action, int mods);
	void handleclick(int key, int action, int mods);
	void handlemouse(double x, double y);
	bool initscene();

	bool quicksetup();

private:
	vkobjs mvkobjs{};


	std::chrono::high_resolution_clock::time_point starttime = std::chrono::high_resolution_clock::now();

	vkcam mcam{};


	std::shared_ptr<playoutplayer> mplayer;

	std::vector<std::shared_ptr<playoutmodel>> mpgltf;

	std::vector<std::shared_ptr<playoutstatic>> mstatic0;

	bool mmodeluploadrequired{ true };



	bool rdscene{ true };

	bool mlock{};
	int mousex{ 0 };
	int mousey{ 0 };
	double mlasttick{ 0.0 };
	void movecam();
	int mcamforward{ 0 };
	int mcamstrafe{ 0 };
	int mcamupdown{ 0 };

	unsigned int playercount{ 1 };
	const std::vector<unsigned int> animcounts{ 2,4 };
	const std::vector<unsigned int> staticcounts{ 2,60,1 };
	std::string playerfname{ "player.glb" };
	const std::vector<std::string> animfname{ "untitled.glb","untitled1.glb" };
	const std::vector<std::string> staticfname{ "dontuse1.glb","dontuse2.glb","dontuse3.glb" };
	const std::vector<std::string> playershaders{ "shader/gltf_gpu.vert.spv", "shader/gltf_gpu.frag.spv" };
	const std::vector<std::string> animshaders{ "shader/gltf_gpu.vert.spv", "shader/gltf_gpu.frag.spv" };
	const std::vector<std::string> staticshaders{ "shader/static.vert.spv", "shader/static.frag.spv" };


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


	bool initvma();

	bool recreateswapchain();




};