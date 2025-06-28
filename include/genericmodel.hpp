#pragma once
#include "vkclip.hpp"
#include "vknode.hpp"
#include "core/rvk.hpp"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

struct gltfnodedata {
	std::shared_ptr<vknode> rootnode;
	std::vector<std::shared_ptr<vknode>> nodelist;
};

class genericmodel {
public:
	bool loadmodel(rvk &objs, std::string fname);
	void draw(rvk &objs);
	void drawinstanced(rvk &objs, VkPipelineLayout &vkplayout, VkPipeline &vkpline, VkPipeline &vkplineuint,
	                   int instancecount, int stride);
	void cleanup(rvk &objs);
	void uploadvboebo(rvk &objs, VkCommandBuffer &cbuffer);
	std::vector<texdata> gettexdata();
	texdataset gettexdatapls();
	std::string getmodelfname();
	int getnodecount();
	gltfnodedata getgltfnodes();
	size_t gettricount(size_t i, size_t j);

	std::vector<glm::mat4> getinversebindmats();
	std::vector<unsigned int> getnodetojoint();

	std::vector<std::shared_ptr<vkclip>> getanimclips();

	void resetnodedata(std::shared_ptr<vknode> treenode);

private:
	std::vector<bool> meshjointtype{};

	std::vector<unsigned int> jointuintofx{0};

	void createvboebo(rvk &objs);

	void getjointdata();
	void getweightdata();
	void getinvbindmats();
	void getanims();
	void getnodes(std::shared_ptr<vknode> treenode);
	void getnodedata(std::shared_ptr<vknode> treenode);

	std::vector<std::shared_ptr<vknode>> getnodelist(std::vector<std::shared_ptr<vknode>> &nlist, int nodenum);

	int mjnodecount{0};

	fastgltf::Asset mmodel2;

	const std::unordered_map<fastgltf::AccessorType, uint8_t> numotypes{
		{fastgltf::AccessorType::Vec2, 2},   {fastgltf::AccessorType::Vec3, 3},	{fastgltf::AccessorType::Vec4, 4},
		{fastgltf::AccessorType::Mat2, 4},   {fastgltf::AccessorType::Mat3, 9},	{fastgltf::AccessorType::Mat4, 16},
		{fastgltf::AccessorType::Scalar, 1}, {fastgltf::AccessorType::Invalid, 0}};

	std::vector<glm::mat4> minversebindmats{};

	std::vector<std::vector<std::vector<int>>> mattribaccs{};
	std::vector<unsigned int> mnodetojoint{};

	std::vector<std::shared_ptr<vkclip>> manimclips{};

	vkgltfobjs mgltfobjs{};

	std::vector<unsigned short> jointz{};
	std::vector<unsigned char> jointzchar{};
	std::vector<unsigned int> jointzint{};

	unsigned int jointofx{0};

	std::map<std::string, unsigned int> atts = {
		{"POSITION", 0}, {"NORMAL", 1}, {"TEXCOORD_0", 2}, {"JOINTS_0", 3}, {"WEIGHTS_0", 4}
	};
};
