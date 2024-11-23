#pragma once
#include "env.hpp"
class mouse {
public:
	mouse(std::string filename);
	GLFWcursor* cursor;
private:
	GLFWimage image;
};