#include "mouse.hpp"
mouse::mouse(std::string filename) {
	image.pixels = stbi_load(filename.c_str(), &image.width, &image.height, nullptr, 4);
	cursor = glfwCreateCursor(&image, 0, 0);
	stbi_image_free(image.pixels);
}