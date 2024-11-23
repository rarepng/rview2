#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "vknode.hpp"

vknode::~vknode() {}

std::shared_ptr<vknode> vknode::createroot(int rootNodeNum) {
    std::shared_ptr<vknode> mParentNode = std::make_shared<vknode>();
    mParentNode->num = rootNodeNum;
    return mParentNode;
}

void vknode::addchildren(std::vector<int> childNodes) {
    for (const int childNode : childNodes) {
        std::shared_ptr<vknode> child = std::make_shared<vknode>();
        child->num = childNode;
        child->parentnode = shared_from_this();

        childnodes.push_back(child);
    }
}

void vknode::addchildren(fastgltf::pmr::MaybeSmallVector<size_t, 0> children){
    for (const size_t childNode : children) {
        std::shared_ptr<vknode> child = std::make_shared<vknode>();
        child->num = childNode;
        child->parentnode = shared_from_this();

        childnodes.push_back(child);
    }
}

std::shared_ptr<vknode> vknode::getparent() {
    std::shared_ptr<vknode> pNode = parentnode.lock();
    if (pNode) {
        return pNode;
    }
    return nullptr;
}

std::vector<std::shared_ptr<vknode>> vknode::getchildren() {
    return childnodes;
}

void vknode::updatenodeandchildrenmats() {
    /* update this matrix */
    calculatenodemat();

    /* call recursive for child update */
    for (auto& node : childnodes) {
        if (node) {
            node->updatenodeandchildrenmats();
        }
    }
}

int vknode::getnum() {
    return num;
}

void vknode::setname(std::string n) {
    name = n;
}

std::string vknode::getname() {
    return name;
}

void vknode::setscale(glm::vec3 scale) {
    nodescale = scale;
    nodeblendscale = scale;
    mscalemat = glm::scale(glm::mat4(1.0f), nodeblendscale);
    mlocalmatneedsupdate = true;
}

void vknode::settranslation(glm::vec3 translation) {
    nodetrans = translation;
    nodeblendtrans = translation;
    mtransmat = glm::translate(glm::mat4(1.0f), nodeblendtrans);
    mlocalmatneedsupdate = true;
}

void vknode::setrotation(glm::quat rotation) {
    noderot = rotation;
    nodeblendrot = rotation;
    mrotmat = glm::mat4_cast(nodeblendrot);
    mlocalmatneedsupdate = true;
}

void vknode::blendscale(glm::vec3 scale, float blendFactor) {
    float factor = std::clamp(blendFactor, 0.0f, 1.0f);
    nodeblendscale = scale * factor + nodescale * (1.0f - factor);
    mscalemat = glm::scale(glm::mat4(1.0f), nodeblendscale);
    mlocalmatneedsupdate = true;
}

void vknode::blendtrans(glm::vec3 translation, float blendFactor) {
    float factor = std::clamp(blendFactor, 0.0f, 1.0f);
    nodeblendtrans = translation * factor + nodetrans * (1.0f - factor);
    mtransmat = glm::translate(glm::mat4(1.0f), nodeblendtrans);
    mlocalmatneedsupdate = true;
}

void vknode::blendrot(glm::quat rotation, float blendFactor) {
    float factor = std::clamp(blendFactor, 0.0f, 1.0f);
    nodeblendrot = glm::slerp(noderot, rotation, factor);
    mrotmat = glm::mat4_cast(nodeblendrot);
    mlocalmatneedsupdate = true;
}

void vknode::setwpos(glm::vec3 worldPos) {
    mwpos = worldPos;
    mwtransmat = glm::translate(glm::mat4(1.0f), mwpos);
    mwTRmat = mwtransmat * mwrotmat;
    mlocalmatneedsupdate = true;
    updatenodeandchildrenmats();
}

void vknode::setwrot(glm::vec3 worldRot) {
    mwrot = worldRot;
    mwrotmat = glm::mat4_cast(glm::quat(glm::vec3(
        glm::radians(mwrot.x),
        glm::radians(mwrot.y),
        glm::radians(mwrot.z)
    )));
    mwTRmat = mwtransmat * mwrotmat;
    mlocalmatneedsupdate = true;
    updatenodeandchildrenmats();
}

glm::vec3 vknode::getwpos() {
    return mwpos;
}

void vknode::calculatelocalmat() {
    if (mlocalmatneedsupdate) {
        localmat = mwTRmat * mtransmat * mrotmat * mscalemat;
        mlocalmatneedsupdate = false;
    }
}

void vknode::calculatenodemat() {
    calculatelocalmat();

    if (std::shared_ptr<vknode> pNode = parentnode.lock()) {
        parentnodemat = pNode->getnodematrix();
    }

    nodemat = parentnodemat * localmat;
}

glm::mat4 vknode::getnodematrix() {
    return nodemat;
}

glm::quat vknode::getlocalrot() {
    return nodeblendrot;
}

glm::quat vknode::getglobalrot() {
    glm::quat orientation;
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;


    if (!glm::decompose(nodemat, scale, orientation, translation, skew, perspective)) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    return glm::inverse(orientation);
}

glm::vec3 vknode::getglobalpos() {
    glm::quat orientation;
    glm::vec3 scale;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;

    if (!glm::decompose(nodemat, scale, orientation, translation, skew, perspective)) {
        return glm::vec3(0.0f, 0.0f, 0.0f);
    }

    return translation;
}

void vknode::printtree() {
    for (const auto& childNode : childnodes) {
        vknode::printnodes(childNode, 1);
    }
}

void vknode::printnodes(std::shared_ptr<vknode> node, int indent) {
    std::string indendString = "";
    for (int i = 0; i < indent; ++i) {
        indendString += " ";
    }
    indendString += "-";

    for (const auto& childNode : node->childnodes) {
        vknode::printnodes(childNode, indent + 1);
    }
}
