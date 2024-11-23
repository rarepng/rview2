#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include "vktexture.hpp"
#include "vkobjs.hpp"
#include "playoutmodel.hpp"

class texmodel {
public:
	bool loadmodel(vkobjs& objs);
	void draw(vkobjs& objs, VkPipelineLayout& vkplayout, unsigned int numinstances, double& time, double& time2, double& life);
	void cleanup(vkobjs& objs);
	void uploadvertexbuffers(vkobjs& objs);
	void uploadindexbuffers(vkobjs& objs);
private:
	void createvertexbuffers(vkobjs& objs);
	void createindexbuffers(vkobjs& objs);
	std::vector<glm::vec3> mmodel{ {-1.0f,1.0f,0.0f},{-1.0f,-1.0f,0.0f},{1.0f,-1.0f,0.0f},{1.0f,1.0f,0.0f} };
	std::vector<glm::vec2> mtexcoords{ {-1.0f,1.0f},{-1.0f,-1.0f},{1.0f,-1.0f},{1.0f,1.0f} };
	std::vector<unsigned short> mindices{ 0,1,2,2,3,0 };

	vkgltfobjs mgltfobjs;
};
