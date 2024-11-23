#include "env.hpp"
#include "vkwind.hpp"

//building for windows only
//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

int main() {


	std::unique_ptr<vkwind> w = std::make_unique<vkwind>();
	w->init("vk!!!!!!!!!!!!!!!");
	w->frameupdate();
	w->cleanup();

	return 0;
}