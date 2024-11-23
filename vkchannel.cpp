#include "vkchannel.hpp"
void vkchannel::loadchannel(std::shared_ptr<tinygltf::Model> model,const tinygltf::Animation& anim,const tinygltf::AnimationChannel& chann) {
	targetNode = chann.target_node;
	const tinygltf::Accessor& inacc = model->accessors.at(anim.samplers.at(chann.sampler).input);
	const tinygltf::BufferView& inbuffview = model->bufferViews.at(inacc.bufferView);
	const tinygltf::Buffer& inbuff = model->buffers.at(inbuffview.buffer);
	std::vector<float> times{};
	times.reserve(inacc.count);
	times.resize(inacc.count);

	std::memcpy(times.data(), &inbuff.data[inbuffview.byteOffset + inacc.byteOffset], inacc.count*4);
	setTimings(times);
	const tinygltf::AnimationSampler sampler = anim.samplers.at(chann.sampler);

	if (sampler.interpolation.compare("STEP") == 0) {
		interpolationtype0 = interpolationType::STEP;
	}else if (sampler.interpolation.compare("LINEAR") == 0) {
		interpolationtype0 = interpolationType::LINEAR;
	} else {
		interpolationtype0 = interpolationType::CUBICSPLINE;
	}


	const tinygltf::Accessor& outacc = model->accessors.at(anim.samplers.at(chann.sampler).output);
	const tinygltf::BufferView& outbuffview = model->bufferViews.at(outacc.bufferView);
	const tinygltf::Buffer& outbuff = model->buffers.at(outbuffview.buffer);
	if (chann.target_path.compare("rotation") == 0) {
		animtype0 = animType::ROTATION;
		std::vector<glm::quat> rotations0;
		rotations0.reserve(outacc.count);
		rotations0.resize(outacc.count);
		rot.reserve(outacc.count);
		rot.resize(outacc.count);
		
		std::memcpy(rotations0.data(), &outbuff.data[outbuffview.byteOffset + outacc.byteOffset], outacc.count * 4 * 4);
		setRots(rotations0);
	}
	else if (chann.target_path.compare("translation") == 0) {
		animtype0 = animType::TRANSLATION;
		std::vector<glm::vec3> translations;
		translations.reserve(outacc.count);
		translations.resize(outacc.count);
		trans.reserve(outacc.count);
		trans.resize(outacc.count);
		std::memcpy(translations.data(), &outbuff.data[outbuffview.byteOffset + outacc.byteOffset], outacc.count * 4 * 3);
		setTranses(translations);
	}
	else {
		animtype0 = animType::SCALE;
		std::vector<glm::vec3> scalings;
		scalings.reserve(outacc.count);
		scalings.resize(outacc.count);
		scale.reserve(outacc.count);
		scale.resize(outacc.count);
		std::memcpy(scalings.data(), &outbuff.data[outbuffview.byteOffset + outacc.byteOffset], outacc.count * 4 * 3);
		setScales(scalings);
	}
}




void vkchannel::setTimings(std::vector<float> timings) {
	timing = timings;
}
void vkchannel::setScales(std::vector<glm::vec<3, float>> scales) {
	scale = scales;
}
void vkchannel::setTranses(std::vector<glm::vec<3, float>> transes) {
	trans = transes;
}
void vkchannel::setRots(std::vector<glm::qua<float>> rots) {
	rot = rots;
}


int vkchannel::getTargetNode() {
	return targetNode;
}
animType vkchannel::getAnimType() {
	return animtype0;
}






glm::vec<3, float> vkchannel::getScale(float time) {
	if (scale.size() == 0) {
		return glm::vec<3, float>{1.0f};
	}
	if (time < timing.at(0)) {
		return scale.at(0);
	}
	if (time > timing.back()) {
		return scale.back();
	}
	size_t prev{ 0 };
	size_t next{ scale.size()-1};
	size_t mid{ 0 };
	while (prev <= next) {
		mid = (prev + next) / 2;
		if (time > timing.at(mid)) {
			prev = mid + 1;
		} else if (time < timing.at(mid)) {
			next = mid - 1;
		} else {
			break;
		}
	}
	//for (size_t i{ 0 }; i < timing.size(); ++i) {
	//	if (timing.at(i) > time) {
	//		next = i;
	//		break;
	//	}
	//	prev = i;
	//}
	if (prev == next) {
		return scale.at(prev);
	}
	glm::vec3 finscale {1.0f};
	switch (interpolationtype0) {
	case interpolationType::LINEAR:
	{
		float intTime = (time - timing.at(prev)) / (timing.at(next) - timing.at(prev));
		glm::vec<3, float> prevscale{ scale.at(prev) };
		glm::vec<3, float> nextscale{ scale.at(next) };
		finscale = prevscale + intTime * (nextscale - prevscale);
	}
	break;
	case interpolationType::CUBICSPLINE:
	{
		float dtime = timing.at(next) - timing.at(prev);
		glm::vec3 prevtangent = dtime * scale.at(prev * 3 + 2);
		glm::vec3 nexttanget = dtime * scale.at(next * 3);
		float intTime = (time - timing.at(prev)) / (timing.at(next) - timing.at(prev));
		float intTime2 = intTime * intTime;
		float intTime3 = intTime2 * intTime;
		glm::vec3 prevp = scale.at(prev * 3 + 1);
		glm::vec3 nextp = scale.at(next * 3 + 1);
		finscale = (2 * intTime3 - 3 * intTime2 + 1) * prevp + (intTime3 - 2 * intTime2 + intTime) * prevtangent + (-2 * intTime3 + 3 * intTime2) * nextp + (intTime3 - intTime2) * nexttanget;
	}
	break;
	case interpolationType::STEP:
		finscale = scale.at(prev);
		break;
	}
	return finscale;
}
glm::vec<3, float> vkchannel::getTranslate(float time) {
	if (trans.size() == 0) {
		return glm::vec<3, float>{0.0f};
	}
	if (time < timing.at(0)) {
		return trans.at(0);
	}
	if (time > timing.back()) {
		return trans.back();
	}
	size_t prev{ 0 };
	size_t next{ trans.size()-1 };
	size_t mid{ 0 };

	while (prev <= next) {
		mid = (prev + next) / 2;
		if (time > timing.at(mid)) {
			prev = mid + 1;
		}
		else if (time < timing.at(mid)) {
			next = mid - 1;
		}
		else {
			break;
		}
	}



	//for (size_t i{ 0 }; i < timing.size(); ++i) {
	//	if (timing.at(i) > time) {
	//		next = i;
	//		break;
	//	}
	//	prev = i;
	//}
	if (prev == next) {
		return trans.at(prev);
	}
	glm::vec3 fintrans{1.0f};
	switch (interpolationtype0) {
	case interpolationType::LINEAR:
	{
		float intTime = (time - timing.at(prev)) / (timing.at(next) - timing.at(prev));
		glm::vec<3, float> prevtrans{ trans.at(prev) };
		glm::vec<3, float> nexttrans{ trans.at(next) };
		fintrans = prevtrans + intTime * (nexttrans - prevtrans);
	}
	break;
	case interpolationType::CUBICSPLINE:
	{
		float dtime = timing.at(next) - timing.at(prev);
		glm::vec3 prevtangent = dtime * trans.at(prev * 3 + 2);
		glm::vec3 nexttanget = dtime * trans.at(next * 3);
		float intTime = (time - timing.at(prev)) / (timing.at(next) - timing.at(prev));
		float intTime2 = intTime * intTime;
		float intTime3 = intTime2 * intTime;
		glm::vec3 prevp = trans.at(prev * 3 + 1);
		glm::vec3 nextp = trans.at(next * 3 + 1);
		fintrans = (2 * intTime3 - 3 * intTime2 + 1) * prevp + (intTime3 - 2 * intTime2 + intTime) * prevtangent + (-2 * intTime3 + 3 * intTime2) * nextp + (intTime3 - intTime2) * nexttanget;
	}
	break;
	case interpolationType::STEP:
		fintrans = trans.at(prev);
		break;
	}
	return fintrans;
}
glm::qua<float> vkchannel::getRotate(float time) {
	if (rot.size() == 0) {
		return glm::identity<glm::quat>();
	}
	if (time < timing.at(0)) {
		return rot.at(0);
	}
	if (time > timing.back()) {
		return rot.back();
	}
	size_t prev{ 0 };
	size_t next{ rot.size()-1 };
	size_t mid{ 0 };





	while (prev <= next) {
		mid = (prev + next) / 2;
		if (time > timing.at(mid)) {
			prev = mid + 1;
		}
		else if (time < timing.at(mid)) {
			next = mid - 1;
		}
		else {
			break;
		}
	}





	//for (size_t i{ 0 }; i < timing.size(); ++i) {
	//	if (timing.at(i) > time) {
	//		next = i;
	//		break;
	//	}
	//	prev = i;
	//}




	if (prev == next) {
		return rot.at(prev);
	}
	glm::qua<float> finrot{ 1.0f,0.0f,0.0f,0.0f };
	switch (interpolationtype0) {
	case interpolationType::LINEAR:
	{
		float intTime = (time - timing.at(prev)) / (timing.at(next) - timing.at(prev));
		glm::qua<float> prevrot{ rot.at(prev) };
		glm::qua<float> nextrot{ rot.at(next) };
		finrot = glm::slerp(prevrot, nextrot, intTime);
	}
	break;
	case interpolationType::CUBICSPLINE:
	{
		float dtime = timing.at(next) - timing.at(prev);
		glm::quat prevtangent = dtime * rot.at(prev * 3 + 2);
		glm::quat nexttanget = dtime * rot.at(next * 3);
		float intTime = (time - timing.at(prev)) / (timing.at(next) - timing.at(prev));
		float intTime2 = intTime * intTime;
		float intTime3 = intTime2 * intTime;
		glm::quat prevp = rot.at(prev * 3 + 1);
		glm::quat nextp = rot.at(next * 3 + 1);
		finrot = (2 * intTime3 - 3 * intTime2 + 1) * prevp + (intTime3 - 2 * intTime2 + intTime) * prevtangent + (-2 * intTime3 + 3 * intTime2) * nextp + (intTime3 - intTime2) * nexttanget;
	}
	break;
	case interpolationType::STEP:
		finrot = rot.at(prev);
		break;
	}
	return finrot;
}


float vkchannel::getMaxTime() {
	return timing.back();
}