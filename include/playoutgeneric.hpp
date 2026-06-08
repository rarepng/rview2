#pragma once
#include "genericinstance.hpp"
#include "core/rvk.hpp"
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
class playoutgeneric {
public:
	bool loadmodel(rvkbucket &objs, std::string_view fname);
	bool createinstances(rvkbucket &objs, size_t count, bool rand);
	bool createubo(rvkbucket &objs);
	bool createssbomat(rvkbucket &objs);
	bool createssbostatic(rvkbucket &objs);
	static bool createpline(rvkbucket &objs, std::string_view vfile, std::string_view ffile);
	bool setup(rvkbucket &objs, std::string_view fname, size_t count, std::string_view vfile, std::string_view ffile);
	void draw(rvkbucket &objs, uint32_t indirectoffset);
	void updateanims();
	void updatemats();
	void cleanuplines(rvkbucket &objs);
	void cleanupbuffers(rvkbucket &objs);
	void cleanupmodels(rvkbucket &objs);
	void uploadvboebo(rvkbucket &objs, VkCommandBuffer &cbuffer);
	void uploadubossbo(rvkbucket &objs, std::vector<glm::mat4>& cammats, const glm::vec3& campos);

	std::shared_ptr<genericinstance> getinst(int i);

	size_t instcount();


	bool ready{false};

	inline static VkPipeline skinnedpline = VK_NULL_HANDLE;
	inline static VkPipeline skinnedplineuint = VK_NULL_HANDLE;

	inline uint32_t get_modelID() const {
		return m_modelID;
	}
	inline bool is_skinned() const {
		return mgltf->skinned;
	}

	inline uint32_t getindexcount() const {
		if (!mgltf || mgltf->getgltfnodes().nodelist.empty()) return 0;

		return static_cast<uint32_t>(mgltf->gettricount(0, 0) * 3);
	}

	// hardcoded cause separate meshes
	inline uint32_t getfirstindex() const {
		return 0;
	}

	// hardcoded cause separate meshes also this is incorrect as of now
	inline int32_t getvertexoffset() const {
		return 0;
	}

	// temp
	inline VkBuffer get_ebo() const {
		return mgltf->get_ebo_buffer(0, 0);
	}
	void sync_scene_data();

private:

	ssbodata rdjointmatrixssbo{};

	uint32_t m_modelID;

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
