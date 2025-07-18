#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <vector>

#include "genericmodel.hpp"
#include "iksolver.hpp"
#include "modelsettings.hpp"
#include "vkclip.hpp"
#include "vknode.hpp"
#include "core/rvk.hpp"

class genericinstance {
public:
	genericinstance(std::shared_ptr<genericmodel> model, glm::vec3 worldpos, bool randomize = false);
	~genericinstance();
	void resetnodedata();
	void setskeletonsplitnode(int nodenum);
	int getjointmatrixsize();
	int getjointdualquatssize();
	std::vector<glm::mat4> getjointmats();
	std::vector<glm::mat2x4> getjointdualquats();

	void updateanimation();

	void setinstancesettings(modelsettings &settings);
	modelsettings &getinstancesettings();
	void checkforupdates();

	glm::vec3 getwpos();
	glm::quat getwrot();

	void solveik();
	void setinversekindematicsnode(int effectornodenum, int ikchainrootnodenum);
	void setnumikiterations(int iterations);

	glm::vec3 *getinstpos();

	glm::mat4 calcstaticmat() {
	glm::mat4 x = glm::scale(glm::mat4{1.0f}, mmodelsettings.msworldscale);
	x = glm::toMat4(glm::quat(glm::radians(mmodelsettings.msworldrot))) * x;
	x = glm::translate(glm::mat4{ 1.0f }, mmodelsettings.msworldpos) * x;
	return x;
	// glm::mat4 x{ 1.0 };
	// ////glm::mat4 t = glm::translate(x, mmodelsettings.msworldpos) * glm::scale(x, mmodelsettings.msworldscale) * glm::rotate(x, glm::radians(mmodelsettings.msworldrot.y), glm::vec3{ 0.0f,1.0f,0.0f });
	// glm::mat4 t = glm::translate(x, mmodelsettings.msworldpos) * glm::mat4_cast(glm::quat(glm::radians(mmodelsettings.msworldrot))) * glm::scale(x, mmodelsettings.msworldscale);
	// return t;
	}


private:
	void playanimation(int animnum, float speeddivider, float blendfactor, replaydirection direction);
	void playanimation(int srcanimnum, int dstanimnum, float speeddivider, float blendfactor, replaydirection direction);
	void blendanimationframe(int animnum, float time, float blendfactor);
	void crossblendanimationframe(int srcanimnum, int dstanimnum, float time, float blendfactor);

	float getanimendtime(int animnum);

	void updatenodematrices(std::shared_ptr<vknode> treenode);
	void updatejointmatrices(std::shared_ptr<vknode> treenode);
	void updatejointdualquats(std::shared_ptr<vknode> treenode);
	void updateadditivemask(std::shared_ptr<vknode> treenode, int splitnodenum);

	std::shared_ptr<genericmodel> mgltfmodel = nullptr;
	unsigned int mnodecount{0};

	std::shared_ptr<vknode> mrootnode = nullptr;
	std::vector<std::shared_ptr<vknode>> mnodelist{};

	std::vector<std::shared_ptr<vkclip>> manimclips{};
	std::vector<glm::mat4> minversebindmats{};
	std::vector<glm::mat4> mjointmats{};
	std::vector<glm::mat2x4> mjointdualquats{};

	std::vector<unsigned int> mnodetojoint{};

	std::vector<bool> madditiveanimationmask{};
	std::vector<bool> minvertedadditiveanimationmask{};

	modelsettings mmodelsettings{};
	iksolver miksolver{};
	void solveikbyccd(glm::vec3 target);
	void solveikbyfabrik(glm::vec3 target);
};
