#include "vkwind.hpp"
#include <future>
#include <iostream>
#include <thread>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

bool vkwind::init(std::string title) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		return false;
	}
	static const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());

	std::cout << SDL_GetError() << std::endl;

	mwind = SDL_CreateWindow(title.c_str(), 900, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE); // SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT

	if (!mwind) {
		SDL_Quit();
		return false;
	}

	mvkrenderer = std::make_unique<vkrenderer>(mwind, mode, &shutdown, e);

	SDL_SetCursor(SDL_CreateColorCursor(IMG_Load("resources/mouser.png"), 0, 0));
	SDL_SetWindowIcon(mwind, IMG_Load("resources/icon0.png"));

	if (!mvkrenderer->init()) {
		SDL_Quit();
		return false;
	}
	mvkrenderer->setsize(900, 600);

	mui = mvkrenderer->getuihandle();

	return true;
}

void vkwind::frameupdate() {
	if (!shutdown) {
		mvkrenderer->initscene();
		mvkrenderer->quicksetup();
		mvkrenderer->immediate_submit([&](VkCommandBuffer cbuffer) {
			mvkrenderer->uploadfordraw(cbuffer);
        });
		while (!shutdown) {
			if (!mvkrenderer->draw()) {
				break;
			}
			SDL_PollEvent(e);
		}
	}
}

void vkwind::cleanup() {
	mvkrenderer->cleanup();
	SDL_DestroyWindow(mwind);
	SDL_Quit();
}
