#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/core.hpp>


class vknode:public std::enable_shared_from_this<vknode> {
public:
	~vknode();

	static std::shared_ptr<vknode> createroot(int root);
	void addchildren(std::vector<int> children);
	void addchildren(fastgltf::pmr::MaybeSmallVector<size_t,0> children);
	std::vector<std::shared_ptr<vknode>> getchildren();
	int getnum();
	std::shared_ptr<vknode> getparent();

	void setname(std::string n);
	std::string getname();

	void setscale(glm::vec<3, float> sca);
	void settranslation(glm::vec<3, float> tra);
	void setrotation(glm::qua<float> rot);

	void blendscale(glm::vec3 s, float blendfactor);
	void blendtrans(glm::vec3 t, float blendfactor);
	void blendrot(glm::qua<float> r, float blendfactor);


	glm::qua<float> getlocalrot();
	glm::qua<float> getglobalrot();

	glm::vec3 getglobalpos();

	void setwpos(glm::vec3 p);
	glm::vec3 getwpos();
	void setwrot(glm::vec3 r);


	void calculatelocalmat();
	void calculatenodemat();
	glm::mat<4, 4, float> getnodematrix();


	void updatenodeandchildrenmats();

	void printtree();

private:
	void printnodes(std::shared_ptr<vknode> start, int indent);

	int num = 0;
	std::string name;

	std::weak_ptr<vknode> parentnode;
	std::vector<std::shared_ptr<vknode>> childnodes{};

	glm::vec3 mwpos{ 0.0f };
	glm::vec3 mwrot{ 0.0f };

	glm::vec<3, float> nodeblendscale{ 1.0f };
	glm::vec<3, float> nodeblendtrans{ 0.0f };
	glm::qua<float> nodeblendrot{ 1.0f,0.0f,0.0f,0.0f };

	glm::vec<3, float> nodescale{ 1.0f };
	glm::vec<3, float> nodetrans{ 0.0f };
	glm::qua<float> noderot{ 1.0f,0.0f,0.0f,0.0f };

	glm::mat4 mtransmat{ 1.0f };
	glm::mat4 mrotmat{ 1.0f };
	glm::mat4 mscalemat{ 1.0f };

	glm::mat4 mwtransmat{ 1.0f };
	glm::mat4 mwrotmat{ 1.0f };
	glm::mat4 mwTRmat{ 1.0f };

	bool mlocalmatneedsupdate{ true };


	glm::mat<4, 4, float> localmat{ 1.0f };
	glm::mat<4, 4, float> parentnodemat{ 1.0f };
	glm::mat<4, 4, float> nodemat{ 1.0f };
};
