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
#include <SDL3/SDL_main.h>
#include <unistd.h>


[[gnu::force_align_arg_pointer]]
int main(int c, char** v) {

	if constexpr (rview::core::platform::is_android) {
		while (access("/data/local/tmp/dbg_lock", F_OK) == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}

	std::unique_ptr<rvkbucket> mvk = std::make_unique<rvkbucket>();
	rdebug::set_thread_name("main");

	if (vkwind::init("RViewer", *mvk)) {
		if constexpr (rview::core::platform::is_android) {
			SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
			SDL_Log("[rview2] !!!!!!!!!!!!!!!! TEST SDL LOG !!!!!!!!!!!!!!!!");
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[rview2] !!!!!!!! TEST SDL ERROR !!!!!!!!");
			SDL_Log("\n [rview2] primed \n");
		}
		vkwind::frameupdate(*mvk);
		vkwind::cleanup(*mvk);
	} else {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Critical Failure", "Could not find an appropriate device.", nullptr);
		// std::string hold;
		// std::cin >> hold;
		return -1;
	}

	if constexpr (!rview::core::platform::is_android) {
		std::_Exit(0); // !!!!! ONLY EXPLICIT CLEANUP/AFTEREFFECTS !!!!!! considering std::quick_exit
	} else {
		return 0;
	}
}
