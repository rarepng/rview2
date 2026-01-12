#pragma once
#include "genericinstance.hpp"
#include "core/rvk.hpp"
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
class playoutgeneric {
public:
	bool loadmodel(rvkbucket &objs, std::string fname);
	bool createinstances(rvkbucket &objs, size_t count, bool rand);
	bool createubo(rvkbucket &objs);
	bool createssbomat(rvkbucket &objs);
	bool createssbostatic(rvkbucket &objs);
	static bool createskinnedplayout(rvkbucket &objs);
	static bool createpline(rvkbucket &objs, std::string vfile, std::string ffile);
	bool setup(rvkbucket &objs, std::string fname, size_t count, std::string vfile, std::string ffile);
	void draw(rvkbucket &objs);
	void updateanims();
	void updatemats();
	void cleanuplines(rvkbucket &objs);
	void cleanupbuffers(rvkbucket &objs);
	void cleanupmodels(rvkbucket &objs);
	void uploadvboebo(rvkbucket &objs, VkCommandBuffer &cbuffer);
	void uploadubossbo(rvkbucket &objs,std::vector<glm::mat4> &cammats);

	std::shared_ptr<genericinstance> getinst(int i);

	bool ready{false};

	inline static VkPipelineLayout skinnedplayout = VK_NULL_HANDLE;
	inline static VkPipeline skinnedpline = VK_NULL_HANDLE;
	inline static VkPipeline skinnedplineuint = VK_NULL_HANDLE;

private:


	inline static std::vector<ubodata> rdperspviewmatrixubo{};
	ssbodata rdjointmatrixssbo{};

	bool uploadreq{true};

	size_t totaltricount{};
	int numinstancess{};

	int stride{};

	int numdqs{};
	int nummats{};

	std::string mmodelfilename;
	std::shared_ptr<genericmodel> mgltf = nullptr;
	std::vector<std::shared_ptr<genericinstance>> minstances;

	std::vector<glm::mat4> jointmats{};
	std::vector<glm::mat2x4> jointdqs{};
};
