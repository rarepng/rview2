#pragma once
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "vkrenderer.hpp"
#include "ui.hpp"

class vkwind {
public:

	bool upreq{ true };

	bool init(std::string title);
	void framemainmenuupdate();
	void frameupdate();
	void cleanup();
	bool initgame();
    bool initmenu();
	int mh;
	int mw;
    SDL_Event* e{new SDL_Event{}};
    bool shutdown{false};
private:
    SDL_Window* mwind = nullptr;
	std::unique_ptr<vkrenderer> mvkrenderer;
	ui* mui=nullptr;
};
