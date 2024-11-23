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

struct gltfnodedata {
	std::shared_ptr<vknode> rootnode;
	std::vector<std::shared_ptr<vknode>> nodelist;
};

class vkgltfmodel {
public:
	bool loadmodel(vkobjs& objs,std::string fname);
	void draw(vkobjs& objs);
	void drawinstanced(vkobjs& objs, VkPipelineLayout& vkplayout, int instancecount,int stride);
	void cleanup(vkobjs& objs);
	void uploadvertexbuffers(vkobjs& objs);
	void uploadindexbuffers(vkobjs& objs);
	/////////////////////////////////////////////////////////vktexturedata
	std::vector<vktexdata> gettexdata();
	std::string getmodelfname();
	int getnodecount();
	gltfnodedata getgltfnodes();
	int gettricount(int i);

	std::vector<glm::mat4> getinversebindmats();
	std::vector<unsigned int> getnodetojoint();

	std::vector<std::shared_ptr<vkclip>> getanimclips();

	void resetnodedata(std::shared_ptr<vknode>treenode);

private:
	void createvertexbuffers(vkobjs& objs);
	void createindexbuffers(vkobjs& objs);

	void getjointdata();
	void getweightdata();
	void getinvbindmats();
	void getanims();
	void getnodes(std::shared_ptr<vknode> treenode);
	void getnodedata(std::shared_ptr<vknode> treenode);

	std::vector<std::shared_ptr<vknode>> getnodelist(std::vector<std::shared_ptr<vknode>>& nlist, int nodenum);

	int mjnodecount{ 0 };

	std::string mmodelfilename;

	std::shared_ptr<tinygltf::Model> mmodel = nullptr;


	std::vector<unsigned int> mjointvec{};
	std::vector<glm::vec4> mweightvec{};
	std::vector<glm::mat4> minversebindmats{};

	std::vector<std::vector<int>> mattribaccs{};
	std::vector<unsigned int> mnodetojoint{};

	std::vector<std::shared_ptr<vkclip>> manimclips{};


	vkgltfobjs mgltfobjs{};



	std::map<std::string, GLint> atts = { {"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2},{"JOINTS_0",3},{"WEIGHTS_0",4} };
};