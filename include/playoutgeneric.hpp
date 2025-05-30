#pragma once
#include "genericinstance.hpp"
#include "vkobjs.hpp"
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class playoutgeneric {
public:
	bool loadmodel(vkobjs &objs, std::string fname);
	bool createinstances(vkobjs &objs, size_t count, bool rand);
	bool createubo(vkobjs &objs);
	bool createssbomat(vkobjs &objs);
	bool createplayout(vkobjs &objs);
	bool createpline(vkobjs &objs, std::string vfile, std::string ffile);
	bool setup(vkobjs &objs, std::string fname, size_t count, std::string vfile, std::string ffile);
	void draw(vkobjs &objs);
	void updateanims();
	void updatemats();
	void cleanuplines(vkobjs &objs);
	void cleanupbuffers(vkobjs &objs);
	void cleanupmodels(vkobjs &objs);
	void uploadvboebo(vkobjs &objs, VkCommandBuffer &cbuffer);
	void uploadubossbo(vkobjs &objs,std::vector<glm::mat4> &cammats);

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
