#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include "vktexture.hpp"
#include "vknode.hpp"
#include "vkclip.hpp"
#include "vkobjs.hpp"
#include "modelsettings.hpp"
#include "ubo.hpp"
#include "ssbo.hpp"
#include "animmodel.hpp"
#include "playerinstance.hpp"
#include "playout.hpp"
#include "pline.hpp"

class playoutplayer {
public:
	bool loadmodel(vkobjs& objs, std::string fname);
	bool createinstances(vkobjs& objs, int count, bool rand);
	bool createdecayinstances(vkobjs& objs);
	bool createubo(vkobjs& objs);
	bool createssbomat(vkobjs& objs);
	bool createssbouint(vkobjs& objs);
	bool createssbodq(vkobjs& objs);
	bool createssbodecay(vkobjs& objs);
	bool createplayout(vkobjs& objs);
	bool createpline(vkobjs& objs, std::string vfile, std::string ffile);
	bool createpline2(vkobjs& objs, std::string vfile, std::string ffile);
	bool createdecaypline(vkobjs& objs, std::string vfile, std::string ffile);
	bool setup(vkobjs& objs, std::string fname, int count);
	bool setup2(vkobjs& objs, std::string vfile, std::string ffile);
	void draw(vkobjs& objs);
	void drawdecays(vkobjs& objs, double& decaytime, bool* decaying);
	void updateanims();
	void updatemats();
	void freezedecay();
	void cleanuplines(vkobjs& objs);
	void cleanupbuffers(vkobjs& objs);
	void cleanupmodels(vkobjs& objs);
	void uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer);
	void uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats);

	std::shared_ptr<animinstance> getinst(int i);
	std::shared_ptr<animinstance> getdecayinst(int i);


	unsigned int getnuminstances();

	bool ready{ false };


private:



	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout rdgltfpipelinelayout = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipeline = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipelineuint = VK_NULL_HANDLE;
	VkPipeline rdgltfgpudqpipeline = VK_NULL_HANDLE;
	VkPipeline rdgltfgpudqpipelineuint = VK_NULL_HANDLE;
	VkPipeline decaypline = VK_NULL_HANDLE;
	VkPipeline decayplineuint = VK_NULL_HANDLE;



	std::vector<vkubodata> rdperspviewmatrixubo{};
	vkshaderstoragebufferdata rdjointmatrixssbo{};
	vkshaderstoragebufferdata uintssbo{};
	vkshaderstoragebufferdata rdjointdualquatssbo{};
	vkshaderstoragebufferdata rdjointdecay{};

	bool uploadreq{ true };

	int totaltricount{};
	int numinstancess{};

	int stride{};
	int stridedq{};

	int numdqs{};
	int nummats{};

	std::string mmodelfilename;
	std::shared_ptr<animmodel> mgltf = nullptr;
	std::vector < std::shared_ptr < animinstance >> minstances;
	std::vector < std::shared_ptr < animinstance >> decayinstances;



	std::vector<glm::mat4> jointmats{};
	std::vector<glm::mat4> decaymat{};
	std::vector<glm::mat2x4> jointdqs{};


};
