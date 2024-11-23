
#define _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>	
#include <thread>
#include <mutex>
#include "vkwind.hpp"
#include <iostream>

//building for windows only
//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")


//std::unique_ptr<std::mutex> mtx2;


int main() {

	std::unique_ptr<vkwind> w = std::make_unique<vkwind>();
	if (w->init("Random Arena Wars")) {
		w->framemainmenuupdate();
		w->frameupdate();
		w->cleanup();
	}
	else {
		std::string hold;
		std::cin >> hold;
		return -1;
	}

	return 0;
}
