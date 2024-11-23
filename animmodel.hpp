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
#include "vknode.hpp"
#include "vkclip.hpp"
#include "vkobjs.hpp"
#include "modelsettings.hpp"
#include "ubo.hpp"

struct gltfnodedata {
	std::shared_ptr<vknode> rootnode;
	std::vector<std::shared_ptr<vknode>> nodelist;
};

class animmodel {
public:
	bool loadmodel(vkobjs& objs,std::string fname);
	void draw(vkobjs& objs);
	void drawinstanced(vkobjs& objs, VkPipelineLayout& vkplayout, VkPipeline& vkpline, VkPipeline& vkplineuint, int instancecount, int stride);
	void drawinstanced(vkobjs& objs, VkPipelineLayout& vkplayout, int instancecount, int stride,double& decaytime,bool* decaying);
	void cleanup(vkobjs& objs);
	void uploadvertexbuffers(vkobjs& objs);
	void uploadindexbuffers(vkobjs& objs);
	void uploadvboebo(vkobjs& objs);
	std::vector<vktexdata> gettexdata();
	vktexdatapls gettexdatapls();
	std::string getmodelfname();
	int getnodecount();
	gltfnodedata getgltfnodes();
	int gettricount(int i,int j);

	std::vector<glm::mat4> getinversebindmats();
	std::vector<unsigned int> getnodetojoint();

	std::vector<std::shared_ptr<vkclip>> getanimclips();

	void resetnodedata(std::shared_ptr<vknode>treenode);

private:

	std::vector<bool> meshjointtype{};

	std::vector<unsigned int> jointuintofx{0};


	void createvboebo(vkobjs& objs);

	void getjointdata();
	void getweightdata();
	void getinvbindmats();
	void getanims();
	void getnodes(std::shared_ptr<vknode> treenode);
	void getnodedata(std::shared_ptr<vknode> treenode);

	std::vector<std::shared_ptr<vknode>> getnodelist(std::vector<std::shared_ptr<vknode>>& nlist, int nodenum);

	int mjnodecount{ 0 };

	std::string mmodelfilename;

	//std::shared_ptr<tinygltf::Model> mmodel = nullptr;

	fastgltf::Asset mmodel2;

	


	//std::vector<unsigned int> mjointvec{};
	//std::vector<glm::vec4> mweightvec{};
	//std::vector<glm::mat<4,4,unsigned int>> minversebindmats{};
	std::vector<glm::mat4> minversebindmats{};

	std::vector<std::vector<std::vector<int>>> mattribaccs{};
	std::vector<unsigned int> mnodetojoint{};

	std::vector<std::shared_ptr<vkclip>> manimclips{};


	vkgltfobjs mgltfobjs{};

	std::vector<unsigned short> jointz{};
	std::vector<unsigned char> jointzchar{};
	std::vector<unsigned int> jointzint{};

	unsigned int jointofx{ 0 };



	std::map<std::string, GLint> atts = { {"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2},{"JOINTS_0",3},{"WEIGHTS_0",4} };
};
