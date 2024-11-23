#pragma once
#include <string>
#include <vector>
#include <memory>
//#include <tinygltf/tiny_gltf.h>
#include <fastgltf/core.hpp>
#include "vkchannel.hpp"
#include "vknode.hpp"

class vkclip {
public:
	vkclip(std::string name);
	//void addchan(std::shared_ptr<tinygltf::Model> model, const tinygltf::Animation& anim, const tinygltf::AnimationChannel& chann);
	void addchan(const fastgltf::Asset& model, const fastgltf::Animation& anim, const fastgltf::AnimationChannel& chann);
	void setFrame(std::vector<std::shared_ptr<vknode>> nodes,std::vector<bool> additivemask, float time);
	void blendFrame(std::vector<std::shared_ptr<vknode>> nodes, std::vector<bool> additivemask, float time,float blendfactor);
	std::string getName();
	float getEndTime();
private:
	std::vector<std::shared_ptr<vkchannel>> animchannels{};
	std::string animname;
};
