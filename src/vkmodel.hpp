#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
//#include <tinygltf/tiny_gltf.h>
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


struct mbools {
	bool mgltf{false};
	bool mskeletonanim{false};
	bool mtexturefile{ false };
	bool mscreen{false};
};

class vkmodel {
public:
	vkmodel(bool gltf,bool skeletonanim,bool texturefile,bool screen);
	bool loadmodel(vkobjs& objs, std::vector<std::string> fname);
	bool loadmodel(vkobjs& objs);
	void draw(vkobjs& objs);
	void draw(vkobjs& objs, VkPipelineLayout& vkplayout, double& time, double& time2, double& life);
	void drawgltf(vkobjs& objs, VkPipelineLayout& vkplayout, int instancecount, int stride);
	void drawdecay(vkobjs& objs, VkPipelineLayout& vkplayout, int instancecount, int stride, double& decaytime, bool* decaying);
	void cleanup(vkobjs& objs);
	void uploadvertexbuffers(vkobjs& objs);
	void uploadindexbuffers(vkobjs& objs);
	std::vector<vktexdata> gettexdata();
	vktexdatapls gettexdatapls();
	std::string getmodelfname();
	int getnodecount();
	gltfnodedata getgltfnodes();
	int gettricount(int i, int j);

	std::vector<glm::mat4> getinversebindmats();
	std::vector<unsigned int> getnodetojoint();

	std::vector<std::shared_ptr<vkclip>> getanimclips();

	void resetnodedata(std::shared_ptr<vknode>treenode);

private:

	mbools mmbools{};



	std::vector<glm::vec3> mquad{ {-1.0f,1.0f,0.0f},{-1.0f,-1.0f,0.0f},{1.0f,-1.0f,0.0f},{1.0f,1.0f,0.0f} };
	std::vector<glm::vec2> mtexcoords{ {-1.0f,1.0f},{-1.0f,-1.0f},{1.0f,-1.0f},{1.0f,1.0f} };
	std::vector<unsigned short> mindices{ 0,1,2,2,3,0 };

	void createvertexbuffers(vkobjs& objs);
	void createvertexbuffersgltf(vkobjs& objs);
	void createvertexbuffersgltfskelly(vkobjs& objs);
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

	//std::shared_ptr<tinygltf::Model> mmodel = nullptr;


	//std::vector<unsigned int> mjointvec{};

	std::vector<glm::mat4> minversebindmats{};

	std::vector<std::vector<std::vector<int>>> mattribaccs{};
	std::vector<unsigned int> mnodetojoint{};

	std::vector<std::shared_ptr<vkclip>> manimclips;


	vkgltfobjs mgltfobjs{};

	unsigned int jointofx{ 0 };



	std::map<std::string, GLint> atts = { {"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2},{"JOINTS_0",3},{"WEIGHTS_0",4} };
};
