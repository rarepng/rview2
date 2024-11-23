#include "vkwind.hpp"
#include <iostream>
#include <future>
#include <thread>
#include <chrono>

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

	mvkrenderer = std::make_unique<vkrenderer>(mwind,mmonitor,mode);
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


	//mouse
	mouse mmouse{ "resources/mouser.png" };
	GLFWimage iconer;
	iconer.pixels = stbi_load("resources/icon0.png", &iconer.width, &iconer.height, nullptr, 4);
	glfwSetCursor(mwind,mmouse.cursor);
	glfwSetWindowIcon(mwind, 1, &iconer);


	if (!mvkrenderer->init()) {
		glfwTerminate();
		return false;
	}


	return true;

}

void vkwind::framemainmenuupdate(){
	while (!glfwWindowShouldClose(mwind)) {
		if (!mvkrenderer->drawmainmenu()) {
			auto f = std::async(std::launch::async, [&]{
				return mvkrenderer->initscene();
				});
			auto s = f.wait_for(std::chrono::milliseconds(0));
			while (s != std::future_status::ready) {
				mvkrenderer->drawloading();
				s = f.wait_for(std::chrono::milliseconds(0));
				glfwPollEvents();
			}
			break;
		}
		glfwPollEvents();
	}

}

void vkwind::frameupdate() {
	mvkrenderer->quicksetup();
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

bool vkwind::initgame(){
	return true;
}

bool vkwind::initmenu(){

	return true;
}
