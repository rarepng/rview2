#pragma once
#include <memory>
#include <string>
#include <vector>
// #include <tinygltf/tiny_gltf.h>
#include "vkchannel.hpp"
#include "vknode.hpp"

class vkclip {
public:
	vkclip(std::string name);
	void addchan(const fastgltf::Asset &model, const fastgltf::Animation &anim, const fastgltf::AnimationChannel &chann);
	void setFrame(std::vector<std::shared_ptr<vknode>> nodes, std::vector<bool> additivemask, float time);
	void blendFrame(std::vector<std::shared_ptr<vknode>> nodes, std::vector<bool> additivemask, float time,
	                float blendfactor);
	std::string getName();
	float getEndTime();

private:
	std::vector<std::shared_ptr<vkchannel>> animchannels{};
	std::string animname;
};
