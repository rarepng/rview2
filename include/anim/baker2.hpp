#pragma once
#include "anim/flatskelly.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <queue>
#include <vector>

namespace AssetBaker {
    
    inline void ParseNodeTransform(const fastgltf::Node& node, glm::mat4& outMat) {
        std::visit(fastgltf::visitor{
            [&](const fastgltf::TRS& trs) {
                glm::vec3 T = glm::make_vec3(trs.translation.data());
                glm::quat R = glm::make_quat(trs.rotation.data());
                glm::vec3 S = glm::make_vec3(trs.scale.data());
                outMat = glm::translate(glm::mat4(1.0f), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0f), S);
            },
            [&](const auto& matrix) { 
                memcpy(glm::value_ptr(outMat), matrix.data(), sizeof(glm::mat4));
            }
        }, node.transform);
    }

    inline FlatSkeleton BakeSkeleton(const fastgltf::Asset& asset, size_t skinIndex = 0) {
        FlatSkeleton skel{};
        skel.parentIndices.fill(-1);

        if (asset.nodes.empty()) return skel;

        std::vector<int32_t> gltfToTopo(asset.nodes.size(), -1);
        skel.gltfToTopoMap.fill(-1);
        uint32_t topoIdx = 0;
        
        std::vector<bool> isChild(asset.nodes.size(), false);
        for (const auto& node : asset.nodes) {
            for (auto child : node.children) isChild[child] = true;
        }

        std::queue<size_t> q;
        for (size_t i = 0; i < isChild.size(); ++i) {
            if (!isChild[i]) q.push(i);
        }

        while (!q.empty()) {
            size_t nodeIdx = q.front();
            q.pop();

            gltfToTopo[nodeIdx] = topoIdx;

            if (nodeIdx < MAX_BONES) {
                skel.gltfToTopoMap[nodeIdx] = topoIdx;
            }
            
            glm::mat4 localMat(1.0f);
            ParseNodeTransform(asset.nodes[nodeIdx], localMat);
            skel.localTransforms[topoIdx] = localMat;

            for (auto childIdx : asset.nodes[nodeIdx].children) {
                q.push(childIdx);
            }
            topoIdx++;
        }
        
        skel.nodeCount = topoIdx;

        for (size_t i = 0; i < asset.nodes.size(); ++i) {
            int32_t parentTopo = gltfToTopo[i];
            if (parentTopo >= 0) {
                for (auto childIdx : asset.nodes[i].children) {
                    int32_t childTopo = gltfToTopo[childIdx];
                    if (childTopo >= 0) skel.parentIndices[childTopo] = parentTopo;
                }
            }
        }

        if (asset.skins.size() > skinIndex) {
            const auto& skin = asset.skins[skinIndex];
            skel.jointCount = skin.joints.size();

            for (size_t j = 0; j < skel.jointCount; ++j) {
                size_t gltfNodeIdx = skin.joints[j];
                skel.jointToNodeMap[j] = gltfToTopo[gltfNodeIdx];
            }

            if (skin.inverseBindMatrices.has_value()) {
                const auto& acc = asset.accessors[skin.inverseBindMatrices.value()];
                fastgltf::copyFromAccessor<glm::mat4>(asset, acc, skel.inverseBindMatrices.data());
            } else {
                for (size_t j = 0; j < skel.jointCount; ++j) {
                    skel.inverseBindMatrices[j] = glm::mat4(1.0f);
                }
            }
        }

        skel.UpdateGlobalMatrices();

        return skel;
    }
}