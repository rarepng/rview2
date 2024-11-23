#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include "vktexture.hpp"
#include "vkobjs.hpp"

class menubgmodel {
public:
	bool loadmodel(vkobjs& objs);
	void draw(vkobjs& objs, VkPipelineLayout& vkplayout);
	void cleanup(vkobjs& objs);
	void uploadvertexbuffers(vkobjs& objs);
	void uploadindexbuffers(vkobjs& objs);
	//std::vector<vktexdata> gettexdata();
private:
	void createvertexbuffers(vkobjs& objs);
	void createindexbuffers(vkobjs& objs);
	std::vector<glm::vec3> mmodel{ {-1.0f,1.0f,0.0f},{-1.0f,-1.0f,0.0f},{1.0f,-1.0f,0.0f},{1.0f,1.0f,0.0f} };
	std::vector<unsigned short> mindices{0,1,2,2,3,0};

	vkgltfobjs mgltfobjs;
};