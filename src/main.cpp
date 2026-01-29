#include "vkwind.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <iostream>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>

// idk mayble ill drop windows completely instead of having to do this lol
__attribute__((force_align_arg_pointer))
int main(int c, char **v) {

	rvkbucket mvk{};
	if (vkwind::init("RViewer",mvk)) {
		vkwind::frameupdate(mvk);
		vkwind::cleanup(mvk);
	} else {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Critical Failure",
		                         "Could not find an appropriate device.", nullptr);
		// std::string hold;
		// std::cin >> hold;
		return -1;
	}

	return 0;
}
