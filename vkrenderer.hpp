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
#include "pipeline.hpp"
#include "gltfpipeline.hpp"
#include "gltfskeletonpipeline.hpp"
#include "gltfgpupipeline.hpp"
#include "gltfmeshpipeline.hpp"
#include "playout.hpp"
#include "framebuffer.hpp"
#include "commandpool.hpp"
#include "commandbuffer.hpp"
#include "syncobjects.hpp"
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

#include "playoutmodel.hpp"
#include "playoutstatic.hpp"

#include "vkgltfmodel.hpp"
#include "vkgltfinstance.hpp"

//#include "coord.hpp"

#include "vkobjs.hpp"






class vkrenderer {
public:
	vkrenderer(GLFWwindow* wind);
	bool init();
	void setsize(unsigned int w, unsigned int h);
	bool draw();
	void toggleshader();
	void cleanup();
	void handlekey(int key, int scancode, int action, int mods);
	void handleclick(int key, int action, int mods);
	void handlemouse(double x, double y);

private:
	vkobjs mvkobjs{};


	std::chrono::high_resolution_clock::time_point starttime = std::chrono::high_resolution_clock::now();

	vkcam mcam{};

	std::shared_ptr<playoutmodel> mpgltf = nullptr;
	std::shared_ptr<playoutmodel> mpgltf2 = nullptr;

	std::shared_ptr<playoutstatic> mstatic0 = nullptr;

	bool mmodeluploadrequired{ true };



	std::vector<std::shared_ptr<vkgltfinstance>> mgltfinstances{};
	std::vector<std::shared_ptr<vkgltfinstance>> mgltfinstances2{};

	std::vector<std::vector<glm::mat4>> mmodeljointmatrices{};
	std::vector<std::vector<glm::mat2x4>> mmodeljointdualquats{};



	bool mlock{};
	int mousex{ 0 };
	int mousey{ 0 };
	double mlasttick{ 0.0 };
	void movecam();
	int mcamforward{ 0 };
	int mcamstrafe{ 0 };
	int mcamupdown{ 0 };





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

	bool setupmodels();
	bool setupmodels2();
	bool deviceinit();
	bool getqueue();
	bool createdepthbuffer();
	bool createubo();
	bool creatematssbo();
	bool createdqssbo();
	bool createmeshssbo();
	bool createswapchain();
	bool createrenderpass();
	bool setupstaticmodels();
	bool setupstaticmodels2();
	bool creategltfgpupipeline();
	bool creategltfgpudqpipeline();
	bool creategltfmeshpipeline();
	bool createframebuffer();
	bool createcommandpool();
	bool createcommandbuffer();
	bool createsyncobjects();
	bool initui();
	bool loadgltfmodel();
	bool loadgltfmodel2();
	bool createinstances();

	bool initvma();

	bool recreateswapchain();




};