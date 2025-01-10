#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <iostream>
#include "vkwind.hpp"




int main() {

	std::unique_ptr<vkwind> w = std::make_unique<vkwind>();
    if (w->init("RViewer")) {
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
