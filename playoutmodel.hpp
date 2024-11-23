#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include <tinygltf/tiny_gltf.h>
#include "vktexture.hpp"
#include "vknode.hpp"
#include "vkclip.hpp"
#include "vkobjs.hpp"
#include "modelsettings.hpp"
#include "ubo.hpp"
#include "ssbo.hpp"
#include "vkgltfmodel.hpp"
#include "vkgltfinstance.hpp"
#include "playout.hpp"
#include "gltfgpupipeline.hpp"


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
	bool setup(vkobjs& objs, std::string fname);
	bool setup2(vkobjs& objs, std::string vfile,std::string ffile);
	void draw(vkobjs& objs);
	void updateanims();
	void updatemats();
	void cleanuplines(vkobjs& objs);
	void cleanupbuffers(vkobjs& objs);
	void cleanupmodels(vkobjs& objs);
	void uploadvboebo(vkobjs& objs);
	void uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats);

	modelsettings getinstsettings();

	unsigned int getnuminstances();


private:




	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout rdgltfpipelinelayout = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipeline = VK_NULL_HANDLE;
	VkPipeline rdgltfgpudqpipeline = VK_NULL_HANDLE;

	

	std::vector<vkuniformbufferdata> rdperspviewmatrixubo{};
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
	std::shared_ptr<vkgltfmodel> mgltf = nullptr;
	std::vector < std::shared_ptr < vkgltfinstance >> minstances;
	std::shared_ptr<tinygltf::Model> mmodel = nullptr;



	std::vector<glm::mat4> jointmats{};
	std::vector<glm::mat2x4> jointdqs{};


};