
#define _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR //cause of msvc
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <iostream>
#include "vkwind.hpp"

//if building for windows ( msvc )
//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")




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
