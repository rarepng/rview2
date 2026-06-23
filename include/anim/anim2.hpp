#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <core/jobs.hpp>
#include <string_view>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

// in to replace vkclip, vkchannel, vknode [[HOPEFULLY]]
namespace fastgltf {
template <> struct ElementTraits<glm::quat> : ElementTraitsBase<glm::quat, AccessorType::Vec4, float> {};
}

enum class AnimPathType { Translation, Rotation, Scale };
enum class InterpolationType { Step, Linear, CubicSpline };

struct DODRawChannel {
	uint32_t targetNode;
	AnimPathType path;
	InterpolationType interp;
	uint32_t keyCount;
	float* times;
	glm::vec3* vec3Values;
	glm::quat* quatValues;

	glm::vec3 sample_vec3(float time) const {
		if (keyCount == 0) return glm::vec3(0.0f);

		if (time <= times[0]) return vec3Values[0];

		if (time >= times[keyCount - 1]) {
			return (interp == InterpolationType::CubicSpline) ? vec3Values[(keyCount - 1) * 3 + 1] : vec3Values[keyCount - 1];
		}

		uint32_t first = 0;
		uint32_t len = keyCount;

		while (len > 0) {
			uint32_t half = len >> 1;
			uint32_t mid = first + half;

			if (times[mid] <= time) {
				first = mid + 1;
				len -= half + 1;
			} else {
				len = half;
			}
		}

		uint32_t prev = (first > 0) ? first - 1 : 0;
		uint32_t next = (prev + 1 < keyCount) ? prev + 1 : prev;

		if (interp == InterpolationType::Step) return vec3Values[prev];

		float dt = times[next] - times[prev];

		if (dt <= 0.00001f) return vec3Values[prev];

		float t = (time - times[prev]) / dt;

		if (interp == InterpolationType::Linear) {
			return glm::mix(vec3Values[prev], vec3Values[next], t);
		}

		float t2 = t * t;
		float t3 = t2 * t;
		glm::vec3 prevTangent = vec3Values[prev * 3 + 2] * dt;
		glm::vec3 nextTangent = vec3Values[next * 3] * dt;
		glm::vec3 prevP = vec3Values[prev * 3 + 1];
		glm::vec3 nextP = vec3Values[next * 3 + 1];
		return (2.0f * t3 - 3.0f * t2 + 1.0f) * prevP +
		       (t3 - 2.0f * t2 + t) * prevTangent +
		       (-2.0f * t3 + 3.0f * t2) * nextP +
		       (t3 - t2) * nextTangent;
	}

	glm::quat sample_quat(float time) const {
		if (keyCount == 0) return glm::identity<glm::quat>();

		if (time <= times[0]) return quatValues[0];

		if (time >= times[keyCount - 1]) {
			return (interp == InterpolationType::CubicSpline) ? quatValues[(keyCount - 1) * 3 + 1] : quatValues[keyCount - 1];
		}

		uint32_t first = 0;
		uint32_t len = keyCount;

		while (len > 0) {
			uint32_t half = len >> 1;
			uint32_t mid = first + half;

			if (times[mid] <= time) {
				first = mid + 1;
				len -= half + 1;
			} else {
				len = half;
			}
		}

		uint32_t prev = (first > 0) ? first - 1 : 0;
		uint32_t next = (prev + 1 < keyCount) ? prev + 1 : prev;

		if (interp == InterpolationType::Step) return quatValues[prev];

		float dt = times[next] - times[prev];

		if (dt <= 0.00001f) return quatValues[prev];

		float t = (time - times[prev]) / dt;

		if (interp == InterpolationType::Linear) {
			return glm::mix(quatValues[prev], quatValues[next], t);
		}

		float t2 = t * t;
		float t3 = t2 * t;
		glm::quat prevTangent = quatValues[prev * 3 + 2] * dt;
		glm::quat nextTangent = quatValues[next * 3] * dt;
		glm::quat prevP = quatValues[prev * 3 + 1];
		glm::quat nextP = quatValues[next * 3 + 1];

		return (2.0f * t3 - 3.0f * t2 + 1.0f) * prevP +
		       (t3 - 2.0f * t2 + t) * prevTangent +
		       (-2.0f * t3 + 3.0f * t2) * nextP +
		       (t3 - t2) * nextTangent;
	}
};

struct DODRawClip {
	std::string_view name;
	float duration;
	uint32_t channelCount;
	DODRawChannel* channels;
};

// avx512 safe?
inline void* arena_alloc_aligned(ThreadLocalArena& arena, size_t size, size_t alignment = 64) {
	void* raw = arena.allocate(size + alignment);

	if (!raw) {
		throw std::runtime_error("CRITICAL: ARENA OOM.");
	}

	uintptr_t ptr = reinterpret_cast<uintptr_t>(raw);
	uintptr_t offset = alignment - (ptr % alignment);

	if (offset == alignment) offset = 0;

	return reinterpret_cast<void*>(ptr + offset);
}

inline DODRawClip extract_raw_clip(ThreadLocalArena& arena, const fastgltf::Asset& asset, const fastgltf::Animation& anim) {
	DODRawClip clip;
	clip.name = anim.name;
	clip.duration = 0.0f;
	clip.channelCount = static_cast<uint32_t>(anim.channels.size());

	clip.channels = static_cast<DODRawChannel*>(arena_alloc_aligned(arena, clip.channelCount * sizeof(DODRawChannel)));

	float** timeCache = static_cast<float**>(arena_alloc_aligned(arena, asset.accessors.size() * sizeof(float*)));
	std::memset(timeCache, 0, asset.accessors.size() * sizeof(float*));

	void** valCache = static_cast<void**>(arena_alloc_aligned(arena, asset.accessors.size() * sizeof(void*)));
	std::memset(valCache, 0, asset.accessors.size() * sizeof(void*));

	for (uint32_t i = 0; i < clip.channelCount; ++i) {
		const auto& inChan = anim.channels[i];
		const auto& inSamp = anim.samplers[inChan.samplerIndex];

		DODRawChannel& outChan = clip.channels[i];
		outChan.targetNode = inChan.nodeIndex.value();

		if (inSamp.interpolation == fastgltf::AnimationInterpolation::Step) outChan.interp = InterpolationType::Step;
		else if (inSamp.interpolation == fastgltf::AnimationInterpolation::Linear) outChan.interp = InterpolationType::Linear;
		else outChan.interp = InterpolationType::CubicSpline;

		size_t timeAccIdx = inSamp.inputAccessor;
		const auto& timeAcc = asset.accessors[timeAccIdx];
		outChan.keyCount = static_cast<uint32_t>(timeAcc.count);

		if (timeCache[timeAccIdx] == nullptr) {
			timeCache[timeAccIdx] = static_cast<float*>(arena_alloc_aligned(arena, outChan.keyCount * sizeof(float)));
			fastgltf::copyFromAccessor<float>(asset, timeAcc, timeCache[timeAccIdx]);
		}

		outChan.times = timeCache[timeAccIdx]; // reuse - good idea?

		if (outChan.keyCount > 0 && outChan.times[outChan.keyCount - 1] > clip.duration) {
			clip.duration = outChan.times[outChan.keyCount - 1];
		}

		size_t valAccIdx = inSamp.outputAccessor;
		const auto& valAcc = asset.accessors[valAccIdx];

		if (inChan.path == fastgltf::AnimationPath::Rotation) {
			outChan.path = AnimPathType::Rotation;

			if (valCache[valAccIdx] == nullptr) {
				valCache[valAccIdx] = arena_alloc_aligned(arena, valAcc.count * sizeof(glm::quat));
				fastgltf::copyFromAccessor<glm::quat>(asset, valAcc, valCache[valAccIdx]);
			}

			outChan.quatValues = static_cast<glm::quat*>(valCache[valAccIdx]);
			outChan.vec3Values = nullptr;
		} else {
			outChan.path = (inChan.path == fastgltf::AnimationPath::Translation) ? AnimPathType::Translation : AnimPathType::Scale;

			if (valCache[valAccIdx] == nullptr) {
				valCache[valAccIdx] = arena_alloc_aligned(arena, valAcc.count * sizeof(glm::vec3));
				fastgltf::copyFromAccessor<glm::vec3>(asset, valAcc, valCache[valAccIdx]);
			}

			outChan.vec3Values = static_cast<glm::vec3*>(valCache[valAccIdx]);
			outChan.quatValues = nullptr;
		}
	}

	return clip;
}