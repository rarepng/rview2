#include "mouse.hpp"
#include <SDL3_image/SDL_image.h>
mouse::mouse(std::string filename) {
	image = IMG_Load(filename.c_str());
	cursor = SDL_CreateColorCursor(image, 0, 0);
}
