#pragma once
#include "ui.hpp"
#include "vkrenderer.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>

class vkwind {
public:
	bool init(std::string title);
	void frameupdate();
	void cleanup();

	SDL_Event *e{new SDL_Event{}};
	bool shutdown{false};

private:
	SDL_Window *mwind = nullptr;
	std::unique_ptr<vkrenderer> mvkrenderer;
	ui *mui = nullptr;
};
