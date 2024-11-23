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
    //for (const auto& node : mnodes) {
    //    if (node) {
    //        Logger::log(2, "%s: added node %s to IK solver\n", __FUNCTION__,
    //            node->getname().c_str());
    //    }
    //}
    calculatebonelengths();
    mfabriknodeposes.reserve(mnodes.size());
    mfabriknodeposes.resize(mnodes.size());
}

void iksolver::calculatebonelengths() {
    mbonelengths.resize(mnodes.size() - 1);
    for (int i = 0; i < mnodes.size() - 1; ++i) {
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
        /* we are really close to the target, stop iterations */
        glm::vec3 effector = mnodes.at(0)->getglobalpos();
        if (glm::length(target - effector) < mthreshold) {
            return true;
        }

        /* iterate the IK chain from node after effector to the root node */
        for (size_t j = 1; j < mnodes.size(); ++j) {
            std::shared_ptr<vknode> node = mnodes.at(j);
            if (!node) {
                continue;
            }

            /* get the global position and rotation of the node, NOT the local */
            glm::vec3 position = node->getglobalpos();
            glm::quat rotation = node->getglobalrot();

            /* create normalized vec3 from current world position to:
             * - effector
             * - target
             * and calculate the angle we have to rotate the node about */
            glm::vec3 toEffector = glm::normalize(effector - position);
            glm::vec3 toTarget = glm::normalize(target - position);

            glm::quat effectorToTarget = glm::rotation(toEffector, toTarget);

            /* calculate the required local rotation from the world rotation */
            glm::quat localRotation = rotation * effectorToTarget * glm::conjugate(rotation);

            /* rotate the node LOCALLY around the old plus the new rotation */
            glm::quat currentRotation = node->getlocalrot();
            node->blendrot(currentRotation * localRotation, 1.0f);

            /* update the node matrices, current node to effector
               to reflect the local changes down the chain */
            node->updatenodeandchildrenmats();

            /* evaluate effector at the end of every iteration again */
            effector = mnodes.at(0)->getglobalpos();
            if (glm::length(target - effector) < mthreshold) {
                return true;
            }
        }
    }

    return false;
}

/* move bones forward, closer to target */
void iksolver::solvefabrikforward(glm::vec3 target) {
    /* set effector to target */
    mfabriknodeposes.at(0) = target;

    for (size_t i = 1; i < mfabriknodeposes.size(); ++i) {
        glm::vec3 boneDirection =
            glm::normalize(mfabriknodeposes.at(i) - mfabriknodeposes.at(i - 1));
        glm::vec3 offset = boneDirection * mbonelengths.at(i - 1);

        mfabriknodeposes.at(i) = mfabriknodeposes.at(i - 1) + offset;
    }
}

/* move bones backward, back to reach base */
void iksolver::solvefabrikbackward(glm::vec3 base) {
    /* set root node back to (saved) base */
    mfabriknodeposes.at(mfabriknodeposes.size() - 1) = base;

    for (int i = mfabriknodeposes.size() - 2; i >= 0; --i) {
        glm::vec3 boneDirection =
            glm::normalize(mfabriknodeposes.at(i) - mfabriknodeposes.at(i + 1));
        glm::vec3 offset = boneDirection * mbonelengths.at(i);

        mfabriknodeposes.at(i) = mfabriknodeposes.at(i + 1) + offset;
    }
}

/* we need to ROTATE the bones, starting with the root node */
void iksolver::adjustfabriknodes() {
    for (size_t i = mfabriknodeposes.size() - 1; i > 0; --i) {
        std::shared_ptr<vknode> node = mnodes.at(i);
        std::shared_ptr<vknode> nextNode = mnodes.at(i - 1);

        /* get the global position and rotation of the original nodes */
        glm::vec3 position = node->getglobalpos();
        glm::quat rotation = node->getglobalrot();

        /* calculate the vector of the original node direction */
        glm::vec3 nextPosition = nextNode->getglobalpos();
        glm::vec3 toNext = glm::normalize(nextPosition - position);

        /* calculate the vector of the changed node direction */
        glm::vec3 toDesired =
            glm::normalize(mfabriknodeposes.at(i - 1) - mfabriknodeposes.at(i));

        /* calculate the angle we have to rotate the node about */
        glm::quat nodeRotation = glm::rotation(toNext, toDesired);

        /* calculate the required local rotation from the world rotation */
        glm::quat localRotation = rotation * nodeRotation * glm::conjugate(rotation);

        /* rotate the node around the old plus the new rotation */
        glm::quat currentRotation = node->getlocalrot();
        node->blendrot(currentRotation * localRotation, 1.0f);

        /* update the node matrices, current node to effector
           to reflect the local changes down the chain */
        node->updatenodeandchildrenmats();
    }
}

bool iksolver::solvefabrik(glm::vec3 target) {
    /* no nodes, no solving possible */
    if (!mnodes.size()) {
        return false;
    }

    /* copy node positions, we will work on the copy */
    for (size_t i = 0; i < mnodes.size(); ++i) {
        std::shared_ptr<vknode> node = mnodes.at(i);
        mfabriknodeposes.at(i) = node->getglobalpos();
    }

    /* get original root node position before altering the bones */
    glm::vec3 base = getikchainrootnode()->getglobalpos();

    for (unsigned int i = 0; i < miterations; ++i) {
        /* we are really close to the target, stop iterations */
        glm::vec3 effector = mfabriknodeposes.at(0);
        if (glm::length(target - effector) < mthreshold) {
            adjustfabriknodes();
            return true;
        }

        /* the solving itself */
        solvefabrikforward(target);
        solvefabrikbackward(base);
    }

    adjustfabriknodes();

    /* return true if we are close to the target */
    glm::vec3 effector = mnodes.at(0)->getglobalpos();
    if (glm::length(target - effector) < mthreshold) {
        return true;
    }

    return false;
}
