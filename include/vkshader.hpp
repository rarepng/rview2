#pragma once
#include <string>
#include <vulkan/vulkan.h>

namespace vkshader {
	VkShaderModule loadshader(VkDevice dev, std::string filename);
	std::string loadfiletostr(std::string_view filename);
};