#include <fstream>
#include <cerrno>
#include <cstring>

#include "vkshader.hpp"

VkShaderModule vkshader::loadshader(VkDevice dev, std::string filename) {
	std::string shadertxt;
	shadertxt = loadfiletostr(filename);

	
	

	VkShaderModuleCreateInfo shadercreateinfo{};
	shadercreateinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shadercreateinfo.codeSize = shadertxt.size();
	shadercreateinfo.pCode = reinterpret_cast<const uint32_t*>(shadertxt.c_str());

	VkShaderModule shadermod;
	if (vkCreateShaderModule(dev, &shadercreateinfo, nullptr, &shadermod) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}
	return shadermod;
}
std::string vkshader::loadfiletostr(std::string filename) {
	std::ifstream infile(filename, std::ios::binary);
	std::string str;

	if (infile.is_open()) {
		str.clear();
		infile.seekg(0, std::ios::end);
		str.reserve(infile.tellg());
		infile.seekg(0, std::ios::beg);

		str.assign((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
		infile.close();
	}
	else {
		return std::string();
	}



	if (infile.bad() || infile.fail()) {
		return std::string();
	}






	infile.close();
	return str;
}