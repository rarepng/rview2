#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include <tinygltf/tiny_gltf.h>
#include "vktexture.hpp"
#include "vkobjs.hpp"
#include "modelsettings.hpp"

class vkgltfstatic {
public:
	bool loadmodel(vkobjs& objs, std::string fname);
	void draw(vkobjs& objs);
	void drawinstanced(vkobjs& objs,VkPipelineLayout& vkplayout, int instancecount, int stride);
	void cleanup(vkobjs& objs);
	void uploadvertexbuffers(vkobjs& objs);
	void uploadindexbuffers(vkobjs& objs);
	std::vector<vktexdata> gettexdata();
	std::string getmodelfname();
	int gettricount(int i);
private:
	void createvertexbuffers(vkobjs& objs);
	void createindexbuffers(vkobjs& objs);
	std::string mmodelfilename;
	std::shared_ptr<tinygltf::Model> mmodel = nullptr;
	std::vector<std::vector<int>> mattribaccs{};

	vkgltfobjs mgltfobjs{};

	std::map<std::string, GLint> atts = { {"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2} };
};