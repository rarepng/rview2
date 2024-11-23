#pragma once
#include <string>
#include <vulkan/vulkan.h>

class vkshader {
public:
	static VkShaderModule loadshader(VkDevice dev, std::string filename);
private:
	static std::string loadfiletostr(std::string filename);
};