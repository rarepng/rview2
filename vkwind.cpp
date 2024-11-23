#include "vkwind.hpp"
#include <iostream>
#include <future>
#include <thread>
#include <chrono>
#include <mutex>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_stdlib.h>

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
	static const GLFWvidmode* mode = glfwGetVideoMode(mmonitor);
	mh = mode->height;
	mw = mode->width;
	//mwind = glfwCreateWindow(mw, mh, title.c_str(), mmonitor, nullptr);
	mwind = glfwCreateWindow(900, 600, title.c_str(), nullptr, nullptr);

	if (!mwind) {
		glfwTerminate();
		return false;
	}

	mvkrenderer = std::make_unique<vkrenderer>(mwind,mmonitor,mode);
	glfwSetWindowUserPointer(mwind, mvkrenderer.get());
	//glfwSetWindowUserPointer(mwind, this);


	//glfwSetWindowCloseCallback();
	glfwSetWindowSizeCallback(mwind, [](GLFWwindow* win, int width, int height) {
		auto renderer = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		renderer->setsize(width, height);
	});

	//glfwSetWindowMonitor(mwind, mmonitor, 0, 0, mw, mh, mode->refreshRate);

	glfwSetKeyCallback(mwind, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handlekeymenu(key, scancode, action, mods);
	});




	glfwSetCursorPosCallback(mwind, [](GLFWwindow* win, double x, double y) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handlemouse( x, y);
	});


	//mouse
	mouse mmouse{ "resources/mouser.png" };
	GLFWimage iconer{};
	iconer.pixels = stbi_load("resources/icon0.png", &iconer.width, &iconer.height, nullptr, 4);
	glfwSetCursor(mwind,mmouse.cursor);
	glfwSetWindowIcon(mwind, 1, &iconer);


	if (!mvkrenderer->init()) {
		glfwTerminate();
		return false;
	}
	mvkrenderer->setsize(900, 600);

	mui = mvkrenderer->getuihandle();


	return true;

}

void vkwind::framemainmenuupdate(){
	while (!glfwWindowShouldClose(mwind)) {
		if (!mvkrenderer->drawmainmenu()) {
			auto f = std::async(std::launch::async, [&]{
				return mvkrenderer->initscene();
			});
			if(!mvkrenderer->getnetobjs().offlineplay)
				if (mvkrenderer->getserverclientstatus()) {
					nserver = new netserver(mvkrenderer->getnetobjs().port);
					nserver->SetClientConnectedCallback([&](const ClientInfo& clientInfo) { clientconnectcallback(clientInfo); });
					nserver->SetClientDisconnectedCallback([&](const ClientInfo& clientInfo) { clientdisconnectcallback(clientInfo); });
					nserver->SetDataReceivedCallback([&](const ClientInfo& clientInfo,const netbuffer& buffer) { datareccallback(clientInfo,buffer); });
						nserver->Start();
				}
				else {
					nclient = new netclient();
					nclient->SetServerConnectedCallback([&]() {connectedtoservercallback(); });
					nclient->SetServerDisconnectedCallback([&]() {disconnectedfromservercallback(); });
					nclient->SetDataReceivedCallback([&](const netbuffer& buffer) {clientreceiveddatacallback(buffer); });


					nclient->ConnectToServer(mvkrenderer->getnetobjs().serveraddress);

				}
			while (f.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready) {
				mvkrenderer->drawloading();
				glfwPollEvents();
			}
			break;
		}
		glfwPollEvents();
	}
	mvkrenderer->drawblank();
	ImGui_ImplGlfw_RestoreCallbacks(mwind);
	glfwSetMouseButtonCallback(mwind, [](GLFWwindow* win, int key, int action, int mods) {
		auto r = static_cast<vkrenderer*>(glfwGetWindowUserPointer(win));
		r->handleclick(key, action, mods);
	});
	ImGui_ImplGlfw_InstallCallbacks(mwind);
	auto f = std::async(std::launch::async, [&] {
		mvkrenderer->cleanmainmenu();
		mvkrenderer->cleanloading();
		return true;
	});
}

void vkwind::frameupdate() {
	if (!glfwWindowShouldClose(mwind)) {
		if (mvkrenderer->getserverclientstatus())
			mvkrenderer->quicksetup(nserver);
		else
			mvkrenderer->quicksetup(nclient);
		mvkrenderer->uploadfordraw();
		std::thread gamelogic = std::thread([&] { mvkrenderer->gametick(); });
		while (!glfwWindowShouldClose(mwind)) {
			if (gamestate::getstate() == gamestate0::normal) {
				if (!mvkrenderer->draw()) {
					break;
				}
			} else if (gamestate::getstate() == gamestate0::won) {
				gamestate::resetwave();
			} else if (gamestate::getstate() == gamestate0::dead) {
				break;
			} else if (gamestate::getstate() == gamestate0::menu) {
				if(upreq){
					mvkrenderer->initshop();
					mvkrenderer->uploadforshop();
					upreq = false;
				}
				mvkrenderer->drawshop();
			}
			glfwPollEvents();
		}
		gamelogic.join();
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

void vkwind::clientconnectcallback(const ClientInfo& clientInfo){
	std::string out = "client connected : " + clientInfo.ConnectionDesc;
	mui->addchat(out);
}

void vkwind::clientdisconnectcallback(const ClientInfo& clientInfo){
	std::string out = "client disconnected : " + clientInfo.ConnectionDesc;
	mui->addchat(out);
}

void vkwind::datareccallback(const ClientInfo& clientInfo, const netbuffer& buffer){
	std::string out;
	out.reserve(buffer.s);
	out.resize(buffer.s);
	for (size_t i{ 0 }; i < buffer.s; i++) {
		out[i] = *(((char*)buffer.d) + i);
	}

	for (const auto i : nserver->GetConnectedClients()) {
		nserver->SendStringToClient(i.first, out);
	}
	mui->addchat(out);
	std::cout << out << std::endl;
}

void vkwind::connectedtoservercallback(){
	std::string out = "successfully connected to host ";
	mui->addchat(out);
}

void vkwind::disconnectedfromservercallback(){
	std::string out = " disconnected from host ";
	mui->addchat(out);
}

void vkwind::clientreceiveddatacallback(const netbuffer& buffer){
	std::string out;
	out.reserve(buffer.s);
	out.resize(buffer.s);
	for (size_t i{ 0 }; i < buffer.s; i++) {
		out[i] = *(((char*)buffer.d)+i);
	}
	mui->addchat(out);
	std::cout << out << std::endl;
}
