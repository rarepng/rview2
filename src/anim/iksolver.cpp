#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/quaternion.hpp>

#include "iksolver.hpp"

iksolver::iksolver() : iksolver(10) {}

iksolver::iksolver(unsigned int iterations) : miterations(iterations) {}

void iksolver::setnumiterations(unsigned int iterations) {
	miterations = iterations;
}

void iksolver::setnodes(std::vector<std::shared_ptr<vknode>> nodes) {
	mnodes = nodes;
	calculatebonelengths();
	mfabriknodeposes.reserve(mnodes.size());
	mfabriknodeposes.resize(mnodes.size());
}

void iksolver::calculatebonelengths() {
	mbonelengths.resize(mnodes.size() - 1);
	for (size_t i{0}; i < mnodes.size() - 1; ++i) {
		std::shared_ptr<vknode> startNode = mnodes.at(i);
		std::shared_ptr<vknode> endNode = mnodes.at(i + 1);

		glm::vec3 startNodePos = startNode->getglobalpos();
		glm::vec3 endNodePos = endNode->getglobalpos();

		mbonelengths.at(i) = glm::length(endNodePos - startNodePos);
	}
}

std::shared_ptr<vknode> iksolver::getikchainrootnode() {
	return mnodes.at(mnodes.size() - 1);
}

bool iksolver::solveccd(const glm::vec3 target) {
	/* no nodes, no solving possible */
	if (!mnodes.size()) {
		return false;
	}

	for (unsigned int i = 0; i < miterations; ++i) {
		glm::vec3 effector = mnodes.at(0)->getglobalpos();
		if (glm::length(target - effector) < mthreshold) {
			return true;
		}

		for (size_t j = 1; j < mnodes.size(); ++j) {
			std::shared_ptr<vknode> node = mnodes.at(j);
			if (!node) {
				continue;
			}

			glm::vec3 position = node->getglobalpos();
			glm::quat rotation = node->getglobalrot();

			glm::vec3 toEffector = glm::normalize(effector - position);
			glm::vec3 toTarget = glm::normalize(target - position);

			glm::quat effectorToTarget = glm::rotation(toEffector, toTarget);

			glm::quat localRotation = rotation * effectorToTarget * glm::conjugate(rotation);

			glm::quat currentRotation = node->getlocalrot();
			node->blendrot(currentRotation * localRotation, 1.0f);

			node->updatenodeandchildrenmats();

			effector = mnodes.at(0)->getglobalpos();
			if (glm::length(target - effector) < mthreshold) {
				return true;
			}
		}
	}

	return false;
}

void iksolver::solvefabrikforward(glm::vec3 target) {
	/* set effector to target */
	mfabriknodeposes.at(0) = target;

	for (size_t i = 1; i < mfabriknodeposes.size(); ++i) {
		glm::vec3 boneDirection = glm::normalize(mfabriknodeposes.at(i) - mfabriknodeposes.at(i - 1));
		glm::vec3 offset = boneDirection * mbonelengths.at(i - 1);

		mfabriknodeposes.at(i) = mfabriknodeposes.at(i - 1) + offset;
	}
}

void iksolver::solvefabrikbackward(glm::vec3 base) {
	mfabriknodeposes.at(mfabriknodeposes.size() - 1) = base;

	for (int i = mfabriknodeposes.size() - 2; i >= 0; --i) {
		glm::vec3 boneDirection = glm::normalize(mfabriknodeposes.at(i) - mfabriknodeposes.at(i + 1));
		glm::vec3 offset = boneDirection * mbonelengths.at(i);

		mfabriknodeposes.at(i) = mfabriknodeposes.at(i + 1) + offset;
	}
}

void iksolver::adjustfabriknodes() {
	for (size_t i = mfabriknodeposes.size() - 1; i > 0; --i) {
		std::shared_ptr<vknode> node = mnodes.at(i);
		std::shared_ptr<vknode> nextNode = mnodes.at(i - 1);

		glm::vec3 position = node->getglobalpos();
		glm::quat rotation = node->getglobalrot();

		glm::vec3 nextPosition = nextNode->getglobalpos();
		glm::vec3 toNext = glm::normalize(nextPosition - position);

		glm::vec3 toDesired = glm::normalize(mfabriknodeposes.at(i - 1) - mfabriknodeposes.at(i));

		glm::quat nodeRotation = glm::rotation(toNext, toDesired);

		glm::quat localRotation = rotation * nodeRotation * glm::conjugate(rotation);

		glm::quat currentRotation = node->getlocalrot();
		node->blendrot(currentRotation * localRotation, 1.0f);

		node->updatenodeandchildrenmats();
	}
}

bool iksolver::solvefabrik(glm::vec3 target) {
	if (!mnodes.size()) {
		return false;
	}

	for (size_t i = 0; i < mnodes.size(); ++i) {
		std::shared_ptr<vknode> node = mnodes.at(i);
		mfabriknodeposes.at(i) = node->getglobalpos();
	}

	glm::vec3 base = getikchainrootnode()->getglobalpos();

	for (unsigned int i = 0; i < miterations; ++i) {
		glm::vec3 effector = mfabriknodeposes.at(0);
		if (glm::length(target - effector) < mthreshold) {
			adjustfabriknodes();
			return true;
		}

		solvefabrikforward(target);
		solvefabrikbackward(base);
	}

	adjustfabriknodes();

	glm::vec3 effector = mnodes.at(0)->getglobalpos();
	if (glm::length(target - effector) < mthreshold) {
		return true;
	}

	return false;
}
