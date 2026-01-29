#pragma once
#include "ui.hpp"
#include "vkrenderer.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>

namespace vkwind {
bool init(std::string title, rvkbucket& mvkobjs);
void frameupdate(rvkbucket& mvkobjs);
void cleanup(rvkbucket& mvkobjs);
};