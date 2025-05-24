#include "vkwind.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>

// idk how to do this properly
// VKAPI_ATTR VkBool32 VKAPI_CALL dbgcallback(
//     VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
//     VkDebugUtilsMessageTypeFlagsEXT              messageType,
//     const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
//     void*                                        pUserData
// ){
//     std::cout << "vlayer: " << pCallbackData->pMessage << std::endl;
//     return VK_TRUE;
// }

int main() {

	std::unique_ptr<vkwind> w = std::make_unique<vkwind>();
	if (w->init("RViewer")) {
		w->frameupdate();
		w->cleanup();
	} else {
		std::string hold;
		std::cin >> hold;
		return -1;
	}

	return 0;
}
