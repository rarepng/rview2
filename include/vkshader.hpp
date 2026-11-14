#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <fstream>
#include <SDL3/SDL.h>

namespace vkshader {


	inline std::string loadfiletostr_sdl(std::string_view filename) {
		size_t data_size = 0;

		void* data = SDL_LoadFile(filename.data(), &data_size);

		if (!data) {
			SDL_Log("failed to load shader %s: %s", filename.data(), SDL_GetError());
			return "";
		}

		std::string shadertxt(static_cast<char*>(data), data_size);

		SDL_free(data);

		return shadertxt;
	}

	inline std::string loadfiletostr(std::string_view filename) {
		std::ifstream infile(std::string(filename), std::ios::binary);
		std::string str;

		if (infile.is_open()) {
			str.clear();
			infile.seekg(0, std::ios::end);
			str.reserve(infile.tellg());
			infile.seekg(0, std::ios::beg);

			str.assign((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
			infile.close();
		} else {
			return std::string();
		}

		if (infile.bad() || infile.fail()) {
			return std::string();
		}

		infile.close();
		return str;
	}
	inline VkShaderModule loadshader(VkDevice dev, std::string_view filename) {
		std::string shadertxt;
		shadertxt = vkshader::loadfiletostr_sdl(filename);

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
};