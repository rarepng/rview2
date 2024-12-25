#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
//#include <tinygltf/tiny_gltf.h>
#include "vktexture.hpp"
#include "vknode.hpp"
#include "vkclip.hpp"
#include "vkobjs.hpp"
#include "modelsettings.hpp"
#include "ubo.hpp"
#include "ssbo.hpp"
#include "animmodel.hpp"
#include "animinstance.hpp"
#include "playout.hpp"
#include "pline.hpp"


class playoutmodel {
public:
	bool loadmodel(vkobjs& objs, std::string fname);
	bool createinstances(vkobjs& objs,int count, bool rand);
	bool createubo(vkobjs& objs);
	bool createssbomat(vkobjs& objs);
	bool createssbodq(vkobjs& objs);
	bool createplayout(vkobjs& objs);
	bool createpline(vkobjs& objs, std::string vfile, std::string ffile);
	bool createpline2(vkobjs& objs, std::string vfile, std::string ffile);
	bool setup(vkobjs& objs, std::string fname, int count);
	bool setup2(vkobjs& objs, std::string vfile,std::string ffile);
	void draw(vkobjs& objs);
	void updateanims();
	void updatemats();
	void cleanuplines(vkobjs& objs);
	void cleanupbuffers(vkobjs& objs);
	void cleanupmodels(vkobjs& objs);
	void uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer);
	void uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats);

	std::shared_ptr<animinstance> getinst(int i);

	std::vector<std::shared_ptr<animinstance>>& getallinstances();

	modelsettings getinstsettings();

	unsigned int getnuminstances();


private:




	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout rdgltfpipelinelayout = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipeline = VK_NULL_HANDLE;
	VkPipeline rdgltfgpudqpipeline = VK_NULL_HANDLE;

	

	std::vector<vkubodata> rdperspviewmatrixubo{};
	vkshaderstoragebufferdata rdjointmatrixssbo{};
	vkshaderstoragebufferdata rdjointdualquatssbo{};

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
	//std::shared_ptr<tinygltf::Model> mmodel = nullptr;



	std::vector<glm::mat4> jointmats{};
	std::vector<glm::mat2x4> jointdqs{};


};
