#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include "vkobjs.hpp"
#include "ubo.hpp"
#include "ssbo.hpp"
#include "playout.hpp"
#include "pline.hpp"

class playoutref {
public:
	bool init(vkobjs& objs);
	bool createubo(vkobjs& objs);
	bool createssbo(vkobjs& objs);
	bool createplayout(vkobjs& objs);
	bool createpline(vkobjs& objs, std::string vfile, std::string ffile);
	void draw(vkobjs& objs);
	void cleanuplines(vkobjs& objs);
	void cleanupbuffers(vkobjs& objs);
	void uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer);
	void uploadubossbo(vkobjs& objs, std::vector<glm::mat4>& cammats);




private:



	std::vector<VkDescriptorSetLayout> desclayouts{};

	VkPipelineLayout mplayout = VK_NULL_HANDLE;
	VkPipeline mpline = VK_NULL_HANDLE;



	std::vector<vkubodata> rdperspviewmatrixubo{};
	vkshaderstoragebufferdata rdssbo{};

};
