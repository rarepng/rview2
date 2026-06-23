#include <anim/vkchannel.hpp>

namespace fastgltf {
template <> struct ElementTraits<glm::quat> : ElementTraitsBase<glm::quat, AccessorType::Vec4, float> {};
}

void vkchannel::loadchannel(const fastgltf::Asset &model, const fastgltf::Animation &anim,
                            const fastgltf::AnimationChannel &chann) {
	targetNode = chann.nodeIndex.value();
	const fastgltf::Accessor &inacc = model.accessors.at(anim.samplers.at(chann.samplerIndex).inputAccessor);

	std::vector<float> times(inacc.count);
	fastgltf::copyFromAccessor<float>(model, inacc, times.data());
	setTimings(times);

	const fastgltf::AnimationSampler sampler = anim.samplers.at(chann.samplerIndex);

	if (sampler.interpolation == fastgltf::AnimationInterpolation::Step) {
		interpolationtype0 = interpolationType::STEP;
	} else if (sampler.interpolation == fastgltf::AnimationInterpolation::Linear) {
		interpolationtype0 = interpolationType::LINEAR;
	} else {
		interpolationtype0 = interpolationType::CUBICSPLINE;
	}

	const fastgltf::Accessor &outacc = model.accessors.at(anim.samplers.at(chann.samplerIndex).outputAccessor);

	if (chann.path == fastgltf::AnimationPath::Rotation) {
		animtype0 = animType::ROTATION;
		std::vector<glm::quat> rotations0(outacc.count);
		fastgltf::copyFromAccessor<glm::quat>(model, outacc, rotations0.data());
		setRots(rotations0);

		rot.reserve(outacc.count);
		rot.resize(outacc.count);
	} else if (chann.path == fastgltf::AnimationPath::Translation) {
		animtype0 = animType::TRANSLATION;
		std::vector<glm::vec3> translations(outacc.count);
		fastgltf::copyFromAccessor<glm::vec3>(model, outacc, translations.data());
		setTranses(translations);

		trans.reserve(outacc.count);
		trans.resize(outacc.count);
	} else {
		animtype0 = animType::SCALE;
		std::vector<glm::vec3> scalings(outacc.count);
		fastgltf::copyFromAccessor<glm::vec3>(model, outacc, scalings.data());
		setScales(scalings);

		scale.reserve(outacc.count);
		scale.resize(outacc.count);
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
	if (scale.empty()) return glm::vec<3, float> {1.0f};

	const float* t_data = timing.data();

	const glm::vec3* v_data = scale.data();

	size_t count = timing.size();

	if (time <= t_data[0]) return v_data[0];

	if (time >= t_data[count - 1]) {
		return (interpolationtype0 == interpolationType::CUBICSPLINE)
		       ? v_data[(count - 1) * 3 + 1]
		       : v_data[count - 1];
	}

	size_t first = 0;
	size_t len = count;

	while (len > 0) {
		size_t half = len >> 1;
		size_t mid = first + half;

		if (t_data[mid] <= time) {
			first = mid + 1;
			len -= half + 1;
		} else {
			len = half;
		}
	}

	size_t next = first;
	size_t prev = first - 1;

	if (time == t_data[prev]) return v_data[prev];

	switch (interpolationtype0) {
		case interpolationType::LINEAR: {
				float intTime = (time - t_data[prev]) / (t_data[next] - t_data[prev]);
				return v_data[prev] + intTime * (v_data[next] - v_data[prev]);
			}

		case interpolationType::CUBICSPLINE: {
				float dtime = t_data[next] - t_data[prev];
				float intTime = (time - t_data[prev]) / dtime;
				float intTime2 = intTime * intTime;
				float intTime3 = intTime2 * intTime;

				glm::vec3 prevtangent = v_data[prev * 3 + 2] * dtime;
				glm::vec3 nexttanget  = v_data[next * 3]     * dtime;
				glm::vec3 prevp       = v_data[prev * 3 + 1];
				glm::vec3 nextp       = v_data[next * 3 + 1];

				return (2.0f * intTime3 - 3.0f * intTime2 + 1.0f) * prevp +
				       (intTime3 - 2.0f * intTime2 + intTime)     * prevtangent +
				       (-2.0f * intTime3 + 3.0f * intTime2)       * nextp +
				       (intTime3 - intTime2)                      * nexttanget;
			}

		case interpolationType::STEP:
			return v_data[prev];
	}

	return v_data[prev];
}
glm::vec<3, float> vkchannel::getTranslate(float time) {
	if (trans.empty()) return glm::vec<3, float> {0.0f};

	const float* t_data = timing.data();

	const glm::vec3* v_data = trans.data();

	size_t count = timing.size();

	if (time <= t_data[0]) return v_data[0];

	if (time >= t_data[count - 1]) {
		return (interpolationtype0 == interpolationType::CUBICSPLINE)
		       ? v_data[(count - 1) * 3 + 1]
		       : v_data[count - 1];
	}

	size_t first = 0;
	size_t len = count;

	while (len > 0) {
		size_t half = len >> 1;
		size_t mid = first + half;

		if (t_data[mid] <= time) {
			first = mid + 1;
			len -= half + 1;
		} else {
			len = half;
		}
	}

	size_t next = first;
	size_t prev = first - 1;

	if (time == t_data[prev]) return v_data[prev];

	switch (interpolationtype0) {
		case interpolationType::LINEAR: {
				float intTime = (time - t_data[prev]) / (t_data[next] - t_data[prev]);
				return v_data[prev] + intTime * (v_data[next] - v_data[prev]);
			}

		case interpolationType::CUBICSPLINE: {
				float dtime = t_data[next] - t_data[prev];
				float intTime = (time - t_data[prev]) / dtime;
				float intTime2 = intTime * intTime;
				float intTime3 = intTime2 * intTime;

				glm::vec3 prevtangent = v_data[prev * 3 + 2] * dtime;
				glm::vec3 nexttanget  = v_data[next * 3]     * dtime;
				glm::vec3 prevp       = v_data[prev * 3 + 1];
				glm::vec3 nextp       = v_data[next * 3 + 1];

				return (2.0f * intTime3 - 3.0f * intTime2 + 1.0f) * prevp +
				       (intTime3 - 2.0f * intTime2 + intTime)     * prevtangent +
				       (-2.0f * intTime3 + 3.0f * intTime2)       * nextp +
				       (intTime3 - intTime2)                      * nexttanget;
			}

		case interpolationType::STEP:
			return v_data[prev];
	}

	return v_data[prev];
}
glm::qua<float> vkchannel::getRotate(float time) {
	if (rot.empty()) return glm::identity<glm::quat>();

	const float* t_data = timing.data();
	const glm::quat* r_data = rot.data();
	size_t count = timing.size();

	if (time <= t_data[0]) return r_data[0];

	if (time >= t_data[count - 1]) {
		return (interpolationtype0 == interpolationType::CUBICSPLINE)
		       ? r_data[(count - 1) * 3 + 1]
		       : r_data[count - 1];
	}

	size_t first = 0;
	size_t len = count;

	while (len > 0) {
		size_t half = len >> 1;
		size_t mid = first + half;

		if (t_data[mid] <= time) {
			first = mid + 1;
			len -= half + 1;
		} else {
			len = half;
		}
	}

	size_t next = first;
	size_t prev = first - 1;

	if (time == t_data[prev]) return r_data[prev];

	switch (interpolationtype0) {
		case interpolationType::LINEAR: {
				float intTime = (time - t_data[prev]) / (t_data[next] - t_data[prev]);
				return glm::slerp(r_data[prev], r_data[next], intTime);
			}

		case interpolationType::CUBICSPLINE: {
				float dtime = t_data[next] - t_data[prev];
				float intTime = (time - t_data[prev]) / dtime;
				float intTime2 = intTime * intTime;
				float intTime3 = intTime2 * intTime;

				glm::quat prevtangent = r_data[prev * 3 + 2] * dtime;
				glm::quat nexttanget  = r_data[next * 3]     * dtime;
				glm::quat prevp       = r_data[prev * 3 + 1];
				glm::quat nextp       = r_data[next * 3 + 1];

				return (2.0f * intTime3 - 3.0f * intTime2 + 1.0f) * prevp +
				       (intTime3 - 2.0f * intTime2 + intTime)     * prevtangent +
				       (-2.0f * intTime3 + 3.0f * intTime2)       * nextp +
				       (intTime3 - intTime2)                      * nexttanget;
			}

		case interpolationType::STEP:
			return r_data[prev];
	}

	return r_data[prev];
}

float vkchannel::getMaxTime() {
	return timing.back();
}
