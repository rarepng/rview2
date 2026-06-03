#pragma once
#include "vkclip.hpp"
#include "vknode.hpp"
#include "core/rvk.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "anim/flatskelly.hpp"
#include "anim/baker2.hpp"
#include <unordered_map>

struct gltfnodedata {
	std::shared_ptr<vknode> rootnode;
	std::vector<std::shared_ptr<vknode>> nodelist;
};

struct DODAnimationClip {
	std::string name;
	float duration{0.0f};
	float sampleRate{60.0f};
	uint32_t frameCount{0};
	uint32_t nodeCount{0};

	std::vector<glm::mat4> localTransforms;

	inline const glm::mat4* GetFrame(float time, bool loop) const {
		if (duration <= 0.0f || frameCount == 0) return nullptr;

		float t = loop ? std::fmod(time, duration) : std::clamp(time, 0.0f, duration);

		if (t < 0.0f) t += duration;

		uint32_t frameIdx = static_cast<uint32_t>(t * sampleRate);

		if (frameIdx >= frameCount) frameIdx = frameCount - 1;

		return &localTransforms[frameIdx * nodeCount];
	}
};

class genericmodel {
public:
	bool loadmodel(rvkbucket &objs, std::string_view fname);
	void draw(rvkbucket &objs);
	void drawinstanced(rvkbucket &objs, VkPipelineLayout &vkplayout, VkPipeline &vkpline, VkPipeline &vkplineuint,
	                   int instancecount, int stride, uint32_t modelID, uint32_t indirectoffset);
	void cleanup(rvkbucket &objs);
	void uploadvboebo(rvkbucket &objs, VkCommandBuffer &cbuffer);
	std::vector<texdata> gettexdata();
	std::string getmodelfname();
	int getnodecount();
	gltfnodedata getgltfnodes();
	size_t gettricount(size_t i, size_t j);

	std::vector<glm::mat4> getinversebindmats();
	std::vector<unsigned int> getnodetojoint();

	std::vector<std::shared_ptr<vkclip>> getanimclips();

	void resetnodedata(std::shared_ptr<vknode> treenode);
	bool skinned{true};
	FlatSkeleton flatskelly{};
	std::vector<DODAnimationClip> bakedClips;

	//temp
	inline VkBuffer get_ebo_buffer(size_t i, size_t j) {
		return mgltfobjs.ebos[i][j].buffer;
	}


private:
	std::vector<std::vector<bool>> meshjointtype{};

	std::vector<unsigned int> jointuintofx{0};

	void createvboebo(rvkbucket &objs);

	void getjointdata();
	void getinvbindmats();
	void getanims();
	void getnodes(std::shared_ptr<vknode> treenode);
	void getnodedata(std::shared_ptr<vknode> treenode);

	std::vector<std::shared_ptr<vknode>> getnodelist(std::vector<std::shared_ptr<vknode>>& nlist, int nodenum);

	int mjnodecount{0};

	std::vector<uint32_t> mskinJointOffsets{};
	std::vector<uint32_t> mmeshToSkinOffset{};

	void extractmaterials(rvkbucket &objs);

	std::vector<uint32_t> m_global_material_indices;


	fastgltf::Asset mmodel2;

	std::vector<glm::mat4> minversebindmats{};

	std::vector<unsigned int> mnodetojoint{};

	std::vector<std::shared_ptr<vkclip>> manimclips{};

	vkgltfobjs mgltfobjs{};

};
