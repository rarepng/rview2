#pragma once
#include <string>
#include <memory>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "vkrenderer.hpp"
#include "netclient.hpp"
#include "netserver.hpp"
#include "ui.hpp"

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
	//GLFWmonitor* mmonitor;
	int mh;
	int mw;
    SDL_Event* e{new SDL_Event{}};
    bool shutdown{false};
private:
	//void handlekeymenu(int key, int scancode, int action, int mods);
	SDL_Window* mwind = nullptr;
    //SDL_DisplayMode* mmode = nullptr;
	netserver* nserver = nullptr;
	netclient* nclient = nullptr;
	std::unique_ptr<vkrenderer> mvkrenderer;
	ui* mui=nullptr;
};
