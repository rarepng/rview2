#pragma once
#include <GLFW/glfw3.h>
#include <string>
#include <stb/stb_image.h>
class mouse {
public:
	mouse(std::string filename);
	GLFWcursor* cursor;
private:
	GLFWimage image;
};
