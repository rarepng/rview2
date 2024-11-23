#pragma once
#define GLM_ENABLE_EXPIREMENTAL
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "vkgltfmodel.hpp"
#include "vknode.hpp"
#include "vkclip.hpp"
#include "iksolver.hpp"
#include "vkobjs.hpp"
#include "modelsettings.hpp"

class vkgltfinstance {
public:
	vkgltfinstance(std::shared_ptr<vkgltfmodel>model, glm::vec3 worldpos, bool randomize = false);
	~vkgltfinstance();
	void resetnodedata();
	std::shared_ptr<vkmesh> getskeleton();
	void setskeletonsplitnode(int nodenum);
	int getjointmatrixsize();
	int getjointdualquatssize();
	std::vector<glm::mat4> getjointmats();
	std::vector<glm::mat2x4> getjointdualquats();

	void updateanimation();

	void setinstancesettings(modelsettings settings);
	modelsettings getinstancesettings();
	void checkforupdates();

	glm::vec3 getwpos();
	glm::quat getwrot();

	void solveik();
	void setinversekindematicsnode(int effectornodenum, int ikchainrootnodenum);
	void setnumikiterations(int iterations);



private:
	void playanimation(int animnum, float speeddivider, float blendfactor, replaydirection direction);
	void playanimation(int srcanimnum, int dstanimnum, float speeddivider, float blendfactor, replaydirection direction);
	void blendanimationframe(int animnum, float time, float blendfactor);
	void crossblendanimationframe(int srcanimnum, int dstanimnum, float time, float blendfactor);

	float getanimendtime(int animnum);

	void getskeletonpernode(std::shared_ptr<vknode>treenode);
	void updatenodematrices(std::shared_ptr<vknode>treenode);
	void updatejointmatrices(std::shared_ptr<vknode>treenode);
	void updatejointdualquats(std::shared_ptr<vknode>treenode);
	void updateadditivemask(std::shared_ptr<vknode>treenode,int splitnodenum);

	std::shared_ptr<vkgltfmodel> mgltfmodel = nullptr;
	unsigned int mnodecount{ 0 };

	std::shared_ptr<vknode> mrootnode = nullptr;
	std::vector<std::shared_ptr<vknode>> mnodelist{};

	std::vector<std::shared_ptr<vkclip>> manimclips{};
	std::vector<glm::mat4> minversebindmats{};
	std::vector<glm::mat4> mjointmats{};
	std::vector<glm::mat2x4> mjointdualquats{};

	std::vector<unsigned int> mnodetojoint{};

	std::vector<bool> madditiveanimationmask{};
	std::vector<bool> minvertedadditiveanimationmask{};

	std::shared_ptr<vkmesh> mskeletonmesh = nullptr;

	modelsettings mmodelsettings{};
	iksolver miksolver{};
	void solveikbyccd(glm::vec3 target);
	void solveikbyfabrik(glm::vec3 target);

};