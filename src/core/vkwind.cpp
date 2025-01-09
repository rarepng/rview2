#include "vkwind.hpp"
#include "backends/imgui_impl_sdl3.h"
#include <iostream>
#include <future>
#include <thread>
#include <chrono>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <misc/cpp/imgui_stdlib.h>
#include "mouse.hpp"

bool vkwind::init(std::string title) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
		return false;
    }
    static const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay()); // wrong time
    mwind = SDL_CreateWindow(title.c_str(),900, 600,SDL_WINDOW_BORDERLESS | SDL_WINDOW_VULKAN | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_RESIZABLE);

    if (!mwind) {
        SDL_Quit();
		return false;
	}

    mvkrenderer = std::make_unique<vkrenderer>(mwind,mode,&shutdown,e);


	//mouse
	mouse mmouse{ "resources/mouser.png" };
    SDL_Surface* iconer{IMG_Load("resources/icon0.png")};
    SDL_SetCursor(mmouse.cursor);
    SDL_SetWindowIcon(mwind, iconer);


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
        mvkrenderer->uploadfordraw();
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


