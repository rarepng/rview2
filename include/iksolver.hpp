#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "vknode.hpp"

class iksolver {
public:
	iksolver();
	iksolver(unsigned int iterations);
	void setnodes(std::vector<std::shared_ptr<vknode>> nodes);
	std::shared_ptr<vknode> getikchainrootnode();
	void setnumiterations(unsigned int iterations);
	bool solveccd(glm::vec3 target);
	bool solvefabrik(glm::vec3 target);
private:
	std::vector<std::shared_ptr<vknode>> mnodes{};
	std::vector<float> mbonelengths{};
	void calculatebonelengths();
	void solvefabrikforward(glm::vec3 target);
	void solvefabrikbackward(glm::vec3 base);
	void adjustfabriknodes();
	std::vector<glm::vec3> mfabriknodeposes{};
	unsigned int miterations{ 0 };
	float mthreshold = 0.00001f;
};