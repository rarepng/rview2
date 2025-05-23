#pragma once
#include <SDL3/SDL.h>
#include <stb_image.h>
#include <string>
class mouse {
public:
	mouse(std::string filename);
	SDL_Cursor *cursor;

private:
	SDL_Surface *image;
};
