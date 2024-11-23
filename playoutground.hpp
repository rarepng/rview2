#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
//#include <tinygltf/tiny_gltf.h>
#include "vktexture.hpp"
#include "vkobjs.hpp"
#include "staticsettings.hpp"
#include "ubo.hpp"
#include "ssbo.hpp"
#include "staticmodel.hpp"
#include "staticinstance.hpp"
#include "playout.hpp"
#include "pline.hpp"


class playoutground {
public:
	bool loadmodel(vkobjs& objs, std::string fname);
	bool createinstances(vkobjs& objs, int count, bool rand);
	bool createubo(vkobjs& objs);
	bool createssbomat(vkobjs& objs);
	//bool createssbodq(vkobjs& objs);
	bool createplayout(vkobjs& objs);
	bool createpline(vkobjs& objs, std::string vfile, std::string ffile);
	bool setup(vkobjs& objs, std::string fname, int count);
	bool setup2(vkobjs& objs, std::string vfile, std::string ffile);
	void draw(vkobjs& objs);
	//void updateanims();
	void updatemats();
	void cleanuplines(vkobjs& objs);
	void cleanupbuffers(vkobjs& objs);
	void cleanupmodels(vkobjs& objs);
	void uploadvboebo(vkobjs& objs);
	void uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats);


	unsigned int getnuminstances();

	std::shared_ptr<staticinstance> getinst(int x);

private:


	int stride{};

	std::vector<glm::mat4> transmats{};

	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout rdgltfpipelinelayout = VK_NULL_HANDLE;
	VkPipeline rdgltfgpupipeline = VK_NULL_HANDLE;



	std::vector<vkuniformbufferdata> rdperspviewmatrixubo{};
	vkshaderstoragebufferdata rdmodelmatsssbo{};
	//vkshaderstoragebufferdata rdjointdualquatssbo{};

	bool uploadreq{ true };

	int totaltricount{};
	int numinstancess{};


	std::string mmodelfilename;
	std::shared_ptr<staticmodel> mgltf = nullptr;
	std::vector < std::shared_ptr < staticinstance >> minstances;
	//std::shared_ptr<tinygltf::Model> mmodel = nullptr;




};
