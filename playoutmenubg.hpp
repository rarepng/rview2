#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
//#include "vktexture.hpp"
#include "vkobjs.hpp"
//#include "ubo.hpp"
//#include "ssbo.hpp"
#include "playout.hpp"
#include "menubgpipeline.hpp"
#include "menubgmodel.hpp"





class playoutmenubg {
public:
	bool loadmodel(vkobjs& objs);
	//bool createubo(vkobjs& objs);
	//bool createssbomat(vkobjs& objs);
	bool createplayout(vkobjs& objs);
	bool createpline(vkobjs& objs, std::string vfile, std::string ffile);
	bool setup(vkobjs& objs, int count);
	bool setup2(vkobjs& objs, std::string vfile, std::string ffile);
	void draw(vkobjs& objs, double& time, double& time2, double& life);
	void updatemats();
	void cleanuplines(vkobjs& objs);
	void cleanupbuffers(vkobjs& objs);
	void cleanupmodels(vkobjs& objs);
	void uploadvboebo(vkobjs& objs);
	void uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats);

private:

	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout rdgltfpipelinelayout = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipeline = VK_NULL_HANDLE;



	std::vector<vkuniformbufferdata> rdperspviewmatrixubo{};
	vkshaderstoragebufferdata rdmodelmatsssbo{};

	bool uploadreq{ true };

	menubgmodel mmenu{};
};
