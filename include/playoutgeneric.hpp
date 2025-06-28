#pragma once
#include "genericinstance.hpp"
#include "vkobjs.hpp"
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class playoutgeneric {
public:
	bool loadmodel(rvk &objs, std::string fname);
	bool createinstances(rvk &objs, size_t count, bool rand);
	bool createubo(rvk &objs);
	bool createssbomat(rvk &objs);
	bool createplayout(rvk &objs);
	bool createpline(rvk &objs, std::string vfile, std::string ffile);
	bool setup(rvk &objs, std::string fname, size_t count, std::string vfile, std::string ffile);
	void draw(rvk &objs);
	void updateanims();
	void updatemats();
	void cleanuplines(rvk &objs);
	void cleanupbuffers(rvk &objs);
	void cleanupmodels(rvk &objs);
	void uploadvboebo(rvk &objs, VkCommandBuffer &cbuffer);
	void uploadubossbo(rvk &objs,std::vector<glm::mat4> &cammats);

	std::shared_ptr<genericinstance> getinst(int i);

	bool ready{false};

private:
	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout rdgltfpipelinelayout = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipeline = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipelineuint = VK_NULL_HANDLE;

	std::vector<ubodata> rdperspviewmatrixubo{};
	ssbodata rdjointmatrixssbo{};

	bool uploadreq{true};

	size_t totaltricount{};
	int numinstancess{};

	int stride{};

	int numdqs{};
	int nummats{};

	std::string mmodelfilename;
	std::shared_ptr<genericmodel> mgltf = nullptr;
	std::vector<std::shared_ptr<genericinstance>> minstances;

	std::vector<glm::mat4> jointmats{};
	std::vector<glm::mat2x4> jointdqs{};
};
