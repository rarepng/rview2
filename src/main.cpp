#include <vkwind.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <iostream>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
#include <cstdlib>
#include <dbg/trace.hpp>

// idk mayble ill drop windows completely instead of having to do this lol
__attribute__((force_align_arg_pointer))
int main(int c, char** v) {

	std::unique_ptr<rvkbucket> mvk = std::make_unique<rvkbucket>();
	rdebug::set_thread_name("main");

	if (vkwind::init("RViewer", *mvk)) {
		vkwind::frameupdate(*mvk);
		vkwind::cleanup(*mvk);
	} else {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Critical Failure",
		                         "Could not find an appropriate device.", nullptr);
		// std::string hold;
		// std::cin >> hold;
		return -1;
	}

	std::_Exit(0);
}
