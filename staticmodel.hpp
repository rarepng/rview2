#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include "vktexture.hpp"
#include "vkobjs.hpp"
#include "modelsettings.hpp"

class staticmodel {
public:
	bool loadmodel(vkobjs& objs, std::string fname);
	void draw(vkobjs& objs);
	void drawinstanced(vkobjs& objs,VkPipelineLayout& vkplayout, int instancecount, int stride);
	void cleanup(vkobjs& objs);
	void uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer);
	std::vector<vktexdata> gettexdata();
	vktexdatapls gettexdatapls();
	std::string getmodelfname();
	int gettricount(int i,int j);
private:
	void createvboebo(vkobjs& objs);
	std::string mmodelfilename;
	fastgltf::Asset mmodel;
	std::vector<std::vector<std::vector<int>>> mattribaccs{};

	vkgltfobjs mgltfobjs{};

	const std::map<std::string, GLint> atts = { {"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2} };
};
