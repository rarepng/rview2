#pragma once
#include <anim/flatskelly.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <queue>
#include <memory>

namespace AssetBaker {

inline void ParseNodeTransform(const fastgltf::Node& node, glm::mat4& outMat) {
	std::visit(fastgltf::visitor{
		[&](const fastgltf::TRS & trs) {
			glm::vec3 T = glm::make_vec3(trs.translation.data());
			glm::quat R = glm::make_quat(trs.rotation.data());
			glm::vec3 S = glm::make_vec3(trs.scale.data());
			outMat = glm::translate(glm::mat4(1.0f), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0f), S);
		},
		[&](const auto & matrix) {
			memcpy(glm::value_ptr(outMat), matrix.data(), sizeof(glm::mat4));
		}
	}, node.transform);
}

inline void BakeSkeleton(const fastgltf::Asset& asset, ParsedSkeleton& skel, size_t skinIndex = 0) {
	if (asset.nodes.empty()) return;

	uint32_t gltfNodeCount = static_cast<uint32_t>(asset.nodes.size());
	uint32_t jointCount = 0;

	if (asset.skins.size() > skinIndex) {
		jointCount = static_cast<uint32_t>(asset.skins[skinIndex].joints.size());
	}
	skel.allocate(gltfNodeCount, jointCount, gltfNodeCount);

	auto gltfToTopo = std::make_unique<int32_t[]>(gltfNodeCount);
	auto isChild = std::make_unique<bool[]>(gltfNodeCount);

	for (uint32_t i = 0; i < gltfNodeCount; ++i) {
		gltfToTopo[i] = -1;
		isChild[i] = false;
	}

	for (const auto& node : asset.nodes) {
		for (auto child : node.children) isChild[child] = true;
	}

	std::queue<size_t> q;

	for (size_t i = 0; i < gltfNodeCount; ++i) {
		if (!isChild[i]) q.push(i);
	}

	uint32_t topoIdx = 0;

	while (!q.empty()) {
		size_t nodeIdx = q.front();
		q.pop();

		gltfToTopo[nodeIdx] = topoIdx;
		skel.gltfToTopoMap[nodeIdx] = topoIdx;

		glm::mat4 localMat(1.0f);
		ParseNodeTransform(asset.nodes[nodeIdx], localMat);
		skel.localTransforms[topoIdx] = localMat;

		for (auto childIdx : asset.nodes[nodeIdx].children) {
			q.push(childIdx);
		}

		topoIdx++;
	}

	skel.nodeCount = topoIdx;

	for (size_t i = 0; i < gltfNodeCount; ++i) {
		int32_t parentTopo = gltfToTopo[i];

		if (parentTopo >= 0) {
			for (auto childIdx : asset.nodes[i].children) {
				int32_t childTopo = gltfToTopo[childIdx];

				if (childTopo >= 0) skel.parentIndices[childTopo] = parentTopo;
			}
		}
	}

	if (jointCount > 0) {
		const auto& skin = asset.skins[skinIndex];

		for (size_t j = 0; j < jointCount; ++j) {
			size_t gltfNodeIdx = skin.joints[j];
			skel.jointToNodeMap[j] = gltfToTopo[gltfNodeIdx];
		}

		if (skin.inverseBindMatrices.has_value()) {
			const auto& acc = asset.accessors[skin.inverseBindMatrices.value()];

			auto tempMat = std::make_unique<glm::mat4[]>(acc.count);
			fastgltf::copyFromAccessor<glm::mat4>(asset, acc, tempMat.get());

			size_t copyCount = std::min(static_cast<size_t>(acc.count), static_cast<size_t>(jointCount));
			std::memcpy(skel.inverseBindMatrices.get(), tempMat.get(), copyCount * sizeof(glm::mat4));
		}
	}

	skel.UpdateGlobalMatrices();
}
}