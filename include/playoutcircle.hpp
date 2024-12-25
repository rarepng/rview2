#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>

#include "vktexture.hpp"
#include "vkobjs.hpp"
#include "staticsettings.hpp"
#include "ubo.hpp"
#include "ssbo.hpp"
#include "texinstance.hpp"
#include "quadmodel.hpp"
#include "playout.hpp"
#include "pline.hpp"
#include "texmodel.hpp"

class playoutcircle {
public:
	bool loadmodel(vkobjs& objs);
	bool createinstances(vkobjs& objs, int count, bool rand);
	bool createubo(vkobjs& objs);
	bool createssbo(vkobjs& objs);
	bool createplayout(vkobjs& objs, const std::string& tname);
	bool createpline(vkobjs& objs, std::string vfile, std::string ffile);
	bool setup(vkobjs& objs, int count);
	bool setup2(vkobjs& objs, const std::string& tname, std::string vfile, std::string ffile);
	void draw(vkobjs& objs, double& time, double& time2, double& life);
	void updatemats();
	void cleanuplines(vkobjs& objs);
	void cleanupbuffers(vkobjs& objs);
	void cleanupmodels(vkobjs& objs);
	void uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer);
	void uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats, const std::vector<double>& enemylifes);


	unsigned int getnuminstances();

	std::shared_ptr<texinstance> getinst(int x);
	std::vector<std::shared_ptr<texinstance>>& getallinstances();

private:


	int stride{};

	std::vector<glm::mat4> transmats{};

	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout mplayout = VK_NULL_HANDLE;
	VkPipeline mpline = VK_NULL_HANDLE;

	std::vector<double> enemylifebars{};



	std::vector<vkubodata> mubo{};
	vkshaderstoragebufferdata mssbo{};
	vkshaderstoragebufferdata platessbo{};

	bool uploadreq{ true };

	int totaltricount{};
	int numinstancess{};

	vktexdata mtexture{};
	vktexdatapls mtexturepls{};

	std::shared_ptr<texmodel> mquad = nullptr;
	std::vector < std::shared_ptr < texinstance >> minstances;




};
