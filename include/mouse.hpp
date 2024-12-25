#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <stb_image.h>
class mouse {
public:
	mouse(std::string filename);
    SDL_Cursor* cursor;
private:
    SDL_Surface* image;
};
