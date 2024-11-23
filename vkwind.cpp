#include "vkwind.hpp"
#include <iostream>

bool vkwind::init(std::string title) {
	if (!glfwInit()) {
		return false;
	}
	if (!glfwVulkanSupported()) {
		glfwTerminate();
		std::cout << "vulkan not supported";
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	mmonitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(mmonitor);
	mh = mode->height;
	mw = mode->width;
	mwind = glfwCreateWindow(mw, mh, title.c_str(), mmonitor, nullptr);

	if (!mwind) {
		glfwTerminate();
		return false;
	}

	mvkrenderer = std::make_unique<vkrenderer>(mwind);
	glfwSetWindowUserPointer(mwind, mvkrenderer.get());



	//glfwSetWindowSizeCallback(mwind, [](GLFWwindow* win, int width, int height) {
	//	auto renderer = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
	//	renderer->setsize(width, height);
	//	});

	glfwSetWindowMonitor(mwind, mmonitor, 0, 0, mw, mh, mode->refreshRate);

	glfwSetKeyCallback(mwind, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handlekey(key, scancode, action, mods);
	});


	glfwSetMouseButtonCallback(mwind, [](GLFWwindow* win, int key, int action, int mods) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handleclick(key, action, mods);
	});


	glfwSetCursorPosCallback(mwind, [](GLFWwindow* win, double x, double y) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handlemouse( x, y);
	});


	if (!mvkrenderer->init()) {
		glfwTerminate();
		return false;
	}


	//mouse
	mouse mmouse{ "resources/mouser.png" };
	GLFWimage iconer;
	iconer.pixels = stbi_load("resources/icon0.png", &iconer.width, &iconer.height, nullptr, 4);
	glfwSetCursor(mwind,mmouse.cursor);
	glfwSetWindowIcon(mwind, 1, &iconer);




	return true;

}

void vkwind::frameupdate() {
	while (!glfwWindowShouldClose(mwind)) {
		if (!mvkrenderer->draw()) {
			break;
		}
		glfwPollEvents();
	}
}

void vkwind::cleanup() {
	mvkrenderer->cleanup();
	glfwDestroyWindow(mwind);
	glfwTerminate();
}