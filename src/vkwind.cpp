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
		//std::thread gameanimation = std::thread([&] { mvkrenderer->updateanims(); });
		//std::thread checkgameanimation = std::thread([&] { mvkrenderer->checkforanimupdates(); });
		while (!glfwWindowShouldClose(mwind)) {
			if (!mvkrenderer->getnetobjs().offlineplay){
				if (mvkrenderer->getserverclientstatus()) {
					if (gamestate::getstate() == gamestate0::normal) {
						if (!mvkrenderer->draw()) {
							break;
						}
					} else if (gamestate::getstate() == gamestate0::won) {
						if (gamestate::astages.find(0) == gamestate::astages.end()) {
							gamestate::astages.emplace(0, 1);
						}
						++gamestate::astages[0];
						for (const auto i : nserver->GetConnectedClients()) {
							nserver->SendStringToClient(i.first,0, "host in stage " + std::to_string(gamestate::astages[0]));
							nserver->sendgamestate(i.first,0, gamestate::astages[0]);
						}
						mui->addchat("host in stage " + std::to_string(gamestate::astages[0]));
						gamestate::resetwave();
					} else if (gamestate::getstate() == gamestate0::dead) {
						if (gamestate::astages.find(0) == gamestate::astages.end()) {
							gamestate::astages.emplace(0, 1);
						}
						gamestate::astages[0] = -1;
						for (const auto i : nserver->GetConnectedClients()) {
							nserver->SendStringToClient(i.first,0, "host has died");
							nserver->sendgamestate(i.first,0, gamestate::astages[0]);
						}
						gamestate::astages[0] = -1;
						mui->addchat("host has died");
						break;
					} else if (gamestate::getstate() == gamestate0::menu) {
						if (upreq) {
							mvkrenderer->initshop();
							mvkrenderer->uploadforshop();
							upreq = false;
						}
						mvkrenderer->drawshop();
					}
				} else {
					if (gamestate::getstate() == gamestate0::normal) {
						if (!mvkrenderer->draw()) {
							break;
						}
					} else if (gamestate::getstate() == gamestate0::won) {
						nclient->sendgamestate(1);
						gamestate::resetwave();
					} else if (gamestate::getstate() == gamestate0::dead) {
						nclient->sendgamestate(-1);
						break;
					} else if (gamestate::getstate() == gamestate0::menu) {
						if (upreq) {
							mvkrenderer->initshop();
							mvkrenderer->uploadforshop();
							upreq = false;
						}
						mvkrenderer->drawshop();
					}
				}
				} else {
					if (gamestate::getstate() == gamestate0::normal) {
						if (!mvkrenderer->draw()) {
							break;
						}
					} else if (gamestate::getstate() == gamestate0::won) {
						mui->playerwave[0]++;
						gamestate::resetwave();
					} else if (gamestate::getstate() == gamestate0::dead) {
						mui->playerwave[0]=-1;
						mui->addchat("dead");
						break;
					} else if (gamestate::getstate() == gamestate0::menu) {
						if (upreq) {
							mvkrenderer->initshop();
							mvkrenderer->uploadforshop();
							upreq = false;
						}
						mvkrenderer->drawshop();
					}
				}
			glfwPollEvents();
		}
		gamelogic.join();
		//gameanimation.join();
		//checkgameanimation.join();
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
	std::string out = " <has connected> " ;
	std::vector<ClientID> mv;
	auto mm = nserver->GetConnectedClients();
	std::transform(mm.begin(), mm.end(), std::back_inserter(mv), [&](auto& x)->ClientID {return x.second.ID; });
	gamestate::aposes.insert_or_assign(clientInfo.ID, glm::vec3{ 0.0f });
	//std::thread mt([&](){gamestate::addplayer(clientInfo.ID); });
	gamestate::addplayer(clientInfo.ID);
	for (const auto& i : mv) {
		std::cout << i << std::endl;
	}
	for (const auto i : nserver->GetConnectedClients()) {
		nserver->SendStringToClient(i.first, clientInfo.ID, out);
		nserver->sendconnections(i.first, mv);
	}
	mui->addchat(std::to_string(clientInfo.ID)+out);
	//mt.join();
	//mvkrenderer->uploadfordraw();
	mvkrenderer->newconnection = true;
}

void vkwind::clientdisconnectcallback(const ClientInfo& clientInfo){
	std::string out = "<has disconnected>";
	for (const auto i : nserver->GetConnectedClients()) {
		nserver->SendStringToClient(i.first, clientInfo.ID, out);
	}
	mui->addchat(std::to_string(clientInfo.ID) + out);
}

void vkwind::datareccallback(const ClientInfo& clientInfo, const netbuffer& buffer){
	std::string out;
	out.reserve(buffer.s);
	out.resize(buffer.s);
	//out[0] = *(char*)buffer.d;

	for (size_t i{ 0 }; i < buffer.s; i++) {
		out[i] = *(((char*)buffer.d) + i);
	}

	if (out[0] & 1) {
		for (const auto i : nserver->GetConnectedClients()) {
			nserver->SendStringToClient(i.first,clientInfo.ID, out.substr(1));
		}
		mui->addchat(std::to_string(clientInfo.ID) + " :  " + out.substr(1));
		std::cout << out << std::endl;
		out.clear();
	} else if (out[0] & 2) {
		int x = static_cast<int>(*(char*)&out[1]);
		if (x < 0) {
			gamestate::astages[clientInfo.ID] = -1;
			for (const auto i : nserver->GetConnectedClients()) {
				//nserver->SendStringToClient(i.first, clientInfo.ID, std::to_string(clientInfo.ID) + " has died");
				nserver->sendgamestate(i.first, clientInfo.ID, gamestate::astages[clientInfo.ID]);
			}
			mui->addchat(std::to_string(clientInfo.ID) + " has died");
		} else {
			if (gamestate::astages.find(clientInfo.ID)==gamestate::astages.end()) {
				gamestate::astages.emplace(clientInfo.ID, 1);
			}
			++gamestate::astages[clientInfo.ID];
			for (const auto i : nserver->GetConnectedClients()) {
				//nserver->SendStringToClient(i.first, clientInfo.ID, std::to_string(clientInfo.ID) + " is now in stage " + std::to_string(gamestate::astages[clientInfo.ID]));
				nserver->sendgamestate(i.first, clientInfo.ID,gamestate::astages[clientInfo.ID]);
			}
			mui->addchat(std::to_string(clientInfo.ID) + " is now in stage " + std::to_string(gamestate::astages[clientInfo.ID]));
		}
		out.clear();
	} else if (out[0] & 4) {
		//gamestate::aposes.emplace(clientInfo.ID, glm::vec3(*(float*)&out[5], *(float*)&out[9], *(float*)&out[13]));
			gamestate::aposes.insert_or_assign(clientInfo.ID, glm::vec3(*(float*)&out[1], *(float*)&out[5], *(float*)&out[9]));
			//for (const auto i : nserver->GetConnectedClients()) {
				//nserver->sendgamepos(i.first, clientInfo.ID, gamestate::aposes[clientInfo.ID]);
			//}
		out.clear();
	}
}

void vkwind::connectedtoservercallback(){
	std::string out = "successfully connected to host ";
	//auto x= mvkrenderer->getnetobjs().nclient->GetConnectionStatus();
	//std::cout << std::to_string(x);
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
	unsigned int id = *(unsigned int*)&out[1];
	if (out[0] & 1) {
		mui->addchat(std::to_string(id) + " :  " + out.substr(5));
		out.clear();
	} else if (out[0] & 2) {
		int x= static_cast<int>(*(char*)&out[5]);
		if (x<0) {
			if (gamestate::astages.find(id) == gamestate::astages.end()) {
				gamestate::astages.emplace(id, -1);
			}
			gamestate::astages[id] = -1;
			mui->addchat(std::to_string(id) + " has died");
		} else {
			if (gamestate::astages.find(id) == gamestate::astages.end()) {
				gamestate::astages.emplace(id, 1);
			}
			++gamestate::astages[id];
			mui->addchat(std::to_string(id) + " is now in stage " + std::to_string(gamestate::astages[id]));
		}
		out.clear();
	} else if (out[0] & 4) {
		std::span<const float, 3> floatspan(reinterpret_cast<const float*>((out.data() + 5)), 3);
		std::span<const ClientID, 1> idspan(reinterpret_cast<const ClientID*>((out.data() + 1)), 1);
		glm::vec3 x = glm::vec3(floatspan[0], floatspan[1], floatspan[2]);
		//gamestate::aposes.emplace(1, glm::vec3(*(float*)&out[5], *(float*)&out[9], *(float*)&out[13]));
		for (const auto& i : idspan) {
			gamestate::aposes.insert_or_assign(i, x);
			std::cout << i << std::endl;
		}
	} else if (out[0] & 8) {
		std::span<const ClientID> idspan(reinterpret_cast<const ClientID*>((out.data() + 1)), (out.size() - 1)/4);
		//std::vector<ClientID> ids(idspan.begin(),idspan.end());
		for (const auto& i : idspan) {
			if(gamestate::aposes.find(i)==gamestate::aposes.end())
				gamestate::aposes.insert_or_assign(i, glm::vec3{ 0.0f });
			gamestate::addplayer(i);
		}
		mvkrenderer->newconnection = true;
		//mvkrenderer->uploadfordraw();
	}
}
