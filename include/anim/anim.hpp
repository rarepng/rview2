#pragma once
//animation baker ((hopefully))
#include <core/rvk.hpp>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <cstdint>

constexpr uint32_t MAX_BONES = 512;

namespace rview::rvk::anim {

struct AnimHeader {
	uint32_t startOffset;
	uint32_t frameCount;
	float duration;
	float sampleRate;
};

struct alignas(16) PackedBone {
	glm::vec3 translation;
	float pad0;
	glm::vec4 rotation;
	glm::vec3 scale;
	float pad1;
};

struct alignas(16) InstanceState {
	uint32_t animID;
	float time;
	float playbackSpeed;
	uint32_t boneCount;

	uint32_t outputOffset;
	uint32_t headBoneIndex;
	uint32_t padding0;
	uint32_t padding1;

	glm::vec4 lookAtTarget;
};

struct SkeletonDef {
	int32_t parentIndices[MAX_BONES];
	glm::mat4 inverseBindMatrices[MAX_BONES];
};

struct BakedAnimationClip {
	uint32_t startOffset;
	uint32_t frameCount;
	float duration;
	float sampleRate;
	bool looping;
};

class AnimationManager {
public:
	std::unordered_map<std::string, BakedAnimationClip> clips;
};
}