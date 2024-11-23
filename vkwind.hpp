#pragma once
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "vkrenderer.hpp"
#include "mouse.hpp"

class vkwind {
public:
	bool init(std::string title);
	void frameupdate();
	void cleanup();
	GLFWmonitor* mmonitor;
	int mh;
	int mw;
private:
	GLFWwindow* mwind = nullptr;
	std::unique_ptr<vkrenderer> mvkrenderer;
};