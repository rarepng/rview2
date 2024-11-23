#include "vkclip.hpp"
vkclip::vkclip(std::string name) :animname(name) {}
//void vkclip::addchan(std::shared_ptr<tinygltf::Model> model, const tinygltf::Animation& anim, const tinygltf::AnimationChannel& chann) {
//	std::shared_ptr<vkchannel> chan = std::make_shared<vkchannel>();
//	chan->loadchannel(model, anim, chann);
//	animchannels.push_back(chan);
//}
void vkclip::addchan(const fastgltf::Asset& model, const fastgltf::Animation& anim, const fastgltf::AnimationChannel& chann) {
	std::shared_ptr<vkchannel> chan = std::make_shared<vkchannel>();
	chan->loadchannel(model, anim, chann);
	animchannels.push_back(chan);
}
void vkclip::setFrame(std::vector<std::shared_ptr<vknode>> nodes,std::vector<bool> additivemask, float time) {
	for (auto& chann : animchannels) {
		int tnode = chann->getTargetNode();
		if (additivemask.at(tnode)) {
			switch (chann->getAnimType()) {
			case animType::ROTATION:
				nodes.at(tnode)->setrotation(chann->getRotate(time));
				break;
			case animType::SCALE:
				nodes.at(tnode)->setscale(chann->getScale(time));
				break;
			case animType::TRANSLATION:
				nodes.at(tnode)->settranslation(chann->getTranslate(time));
				break;
			}
		}
	}
	for (auto& node0 : nodes) {
		if (node0) {
			node0->calculatelocalmat();
		}
	}
}


void vkclip::blendFrame(std::vector<std::shared_ptr<vknode>> nodes, std::vector<bool> additivemask, float time,float blendfactor) {
	for (auto& chann : animchannels) {
		int tnode = chann->getTargetNode();
		if (additivemask.at(tnode)) {
			switch (chann->getAnimType()) {
			case animType::ROTATION:
				nodes.at(tnode)->blendrot(chann->getRotate(time), blendfactor);
				break;
			case animType::SCALE:
				nodes.at(tnode)->blendscale(chann->getScale(time), blendfactor);
				break;
			case animType::TRANSLATION:
				nodes.at(tnode)->blendtrans(chann->getTranslate(time), blendfactor);
				break;
			}
		}
	}
	for (auto& node0 : nodes) {
		if (node0) {
			node0->calculatelocalmat();
		}
	}
}


std::string vkclip::getName() {
	return animname;
}
float vkclip::getEndTime() {
	return animchannels.at(0)->getMaxTime();
}
