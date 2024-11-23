#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>	
#include <thread>
#include <mutex>
#include "vkwind.hpp"

//building for windows only
//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")


//std::unique_ptr<std::mutex> mtx2;


int main() {

	std::unique_ptr<vkwind> w = std::make_unique<vkwind>();
	w->init("vk!!!!!!!!!!!!!!!");
	w->framemainmenuupdate();
	w->frameupdate();
	w->cleanup();

	return 0;
}