#pragma once
#include <string>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "vkrenderer.hpp"
#include "mouse.hpp"
#include "netclient.hpp"
#include "netserver.hpp"
#include "ui.hpp"
#include "gamestate.hpp"

class vkwind {
public:

	bool upreq{ true };

	bool init(std::string title);
	void framemainmenuupdate();
	void frameupdate();
	void cleanup();
	bool initgame();
	bool initmenu();
	void clientconnectcallback(const ClientInfo& clientInfo);
	void clientdisconnectcallback(const ClientInfo& clientInfo);
	void datareccallback(const ClientInfo& clientInfo, const netbuffer& buffer);
	void connectedtoservercallback();
	void disconnectedfromservercallback();
	void clientreceiveddatacallback(const netbuffer& buffer);
	GLFWmonitor* mmonitor;
	int mh;
	int mw;
private:
	//void handlekeymenu(int key, int scancode, int action, int mods);
	GLFWwindow* mwind = nullptr;
	GLFWvidmode* mmode = nullptr;
	netserver* nserver = nullptr;
	netclient* nclient = nullptr;
	std::unique_ptr<vkrenderer> mvkrenderer;
	ui* mui=nullptr;
};
