#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <string>
#include <vector>
#include <memory>
#include <tinygltf/tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>


enum class animType {
	ROTATION,
	TRANSLATION,
	SCALE
};
enum class interpolationType {
	STEP,
	LINEAR,
	CUBICSPLINE
};
class vkchannel {
public:
	void loadchannel(std::shared_ptr<tinygltf::Model> model,const tinygltf::Animation& anim, const tinygltf::AnimationChannel& chann);
	int getTargetNode();
	animType getAnimType();
	//interpolationType getInterpolationType();
	glm::vec<3, float> getScale(float time);
	glm::vec<3, float> getTranslate(float time);
	glm::qua<float> getRotate(float time);
	float getMaxTime();
private:
	int targetNode = -1;
	animType animtype0 = animType::ROTATION;
	interpolationType interpolationtype0 = interpolationType::LINEAR;
	std::vector<float> timing{};
	std::vector<glm::vec<3, float>> scale{};
	std::vector<glm::vec<3, float>> trans{};
	std::vector<glm::qua<float>> rot{};
	void setTimings(std::vector<float> timings);
	void setScales(std::vector<glm::vec<3, float>> scales);
	void setTranses(std::vector<glm::vec<3, float>> transes);
	void setRots(std::vector<glm::qua<float>> rots);
};