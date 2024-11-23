
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <chrono>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>





#include "vkvbo.hpp"
#include "indexbuffer.hpp"
#include "vkgltfmodel.hpp"

bool vkgltfmodel::loadmodel(vkobjs& objs, std::string fname)
{

    mmodel = std::make_shared<tinygltf::Model>();

    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warr;
    bool res = false;
    res = loader.LoadBinaryFromFile(mmodel.get(), &err, &warr, fname);
    mgltfobjs.rdgltfmodeltex.reserve(mmodel->images.size());
    mgltfobjs.rdgltfmodeltex.resize(mmodel->images.size());
    if (!vktexture::loadtexture(objs, mgltfobjs.rdgltfmodeltex, mmodel))return false;
    if (!vktexture::loadtexlayoutpool(objs, mgltfobjs.rdgltfmodeltex, mmodel))return false;


    mmodelfilename = fname;


    createvertexbuffers(objs);
    createindexbuffers(objs);


    getjointdata();
    getweightdata();
    getinvbindmats();

    mjnodecount = mmodel->nodes.size();

    getanims();

    //mmodel.reset();
    //mmodel.~shared_ptr();

    return true;
}




std::string vkgltfmodel::getmodelfname() {
    return mmodelfilename;
}

int vkgltfmodel::getnodecount() {
    return mjnodecount;
}

gltfnodedata vkgltfmodel::getgltfnodes() {
    gltfnodedata nodeData{};

    int rootNodeNum = mmodel->scenes.at(0).nodes.at(0);

    nodeData.rootnode = vknode::createroot(rootNodeNum);

    getnodedata(nodeData.rootnode);
    getnodes(nodeData.rootnode);

    nodeData.nodelist.reserve(mjnodecount);
    nodeData.nodelist.resize(mjnodecount);
    nodeData.nodelist.at(rootNodeNum) = nodeData.rootnode;
    getnodelist(nodeData.nodelist, rootNodeNum);

    return nodeData;
}

void vkgltfmodel::getjointdata() {
    std::string jointsAccessorAttrib = "JOINTS_0";
    int jointsAccessor = mmodel->meshes.at(0).primitives.at(0).attributes.at(jointsAccessorAttrib);

    const tinygltf::Accessor& accessor = mmodel->accessors.at(jointsAccessor);
    const tinygltf::BufferView& bufferView = mmodel->bufferViews.at(accessor.bufferView);
    const tinygltf::Buffer& buffer = mmodel->buffers.at(bufferView.buffer);

    unsigned int jointVecSize = accessor.count;
    mjointvec.reserve(accessor.count);
    mjointvec.resize(accessor.count);
    //size_t newbytelength{ jointVecSize * bufferView.byteStride };
    size_t newbytelength{ bufferView.byteLength };
    std::memcpy(mjointvec.data(), &buffer.data[bufferView.byteOffset+accessor.byteOffset], newbytelength);

    mnodetojoint.reserve(mmodel->nodes.size());
    mnodetojoint.resize(mmodel->nodes.size());

    const tinygltf::Skin& skin = mmodel->skins.at(0);
    for (unsigned char i = 0; i < skin.joints.size(); ++i) {
        int destinationNode = skin.joints.at(i);
        mnodetojoint.at(destinationNode) = i;
    }
}

void vkgltfmodel::getweightdata() {
    std::string weightsAccessorAttrib = "WEIGHTS_0";
    int weightAccessor = mmodel->meshes.at(0).primitives.at(0).attributes.at(weightsAccessorAttrib);

    const tinygltf::Accessor& accessor = mmodel->accessors.at(weightAccessor);
    const tinygltf::BufferView& bufferView = mmodel->bufferViews.at(accessor.bufferView);
    const tinygltf::Buffer& buffer = mmodel->buffers.at(bufferView.buffer);

    int weightVecSize = accessor.count;
    mweightvec.reserve(weightVecSize);
    mweightvec.resize(weightVecSize);

    //size_t newbytelength{ weightVecSize * bufferView.byteStride };
    size_t newbytelength{ bufferView.byteLength };


    std::memcpy(mweightvec.data(), &buffer.data[bufferView.byteOffset + accessor.byteOffset], newbytelength);
}

void vkgltfmodel::getinvbindmats() {
    const tinygltf::Skin& skin = mmodel->skins.at(0);
    int invBindMatAccessor = skin.inverseBindMatrices;

    const tinygltf::Accessor& accessor = mmodel->accessors.at(invBindMatAccessor);
    const tinygltf::BufferView& bufferView = mmodel->bufferViews.at(accessor.bufferView);
    const tinygltf::Buffer& buffer = mmodel->buffers.at(bufferView.buffer);

    minversebindmats.reserve(skin.joints.size());
    minversebindmats.resize(skin.joints.size());

    size_t newbytelength{ skin.joints.size() * 4 * 4 * 4 };

    std::memcpy(minversebindmats.data(), &buffer.data[bufferView.byteOffset + accessor.byteOffset], newbytelength);
}

void vkgltfmodel::getanims() {
    manimclips.reserve(mmodel->animations.size());
    for (auto& anim0 : mmodel->animations) {
        std::shared_ptr<vkclip> clip0=std::make_shared<vkclip>(anim0.name);
        for (auto& c : anim0.channels) {
            clip0->addchan(mmodel, anim0, c);
        }
        manimclips.push_back(clip0);
    }
}

std::vector<std::shared_ptr<vkclip>> vkgltfmodel::getanimclips() {
    return manimclips;
}

void vkgltfmodel::getnodes(std::shared_ptr<vknode> treeNode) {
    int nodeNum = treeNode->getnum();
    std::vector<int> childNodes = mmodel->nodes.at(nodeNum).children;

    /* remove the child node with skin/mesh metadata */
    auto removeIt = std::remove_if(childNodes.begin(), childNodes.end(),
        [&](int num) { return mmodel->nodes.at(num).skin != -1; }
    );
    childNodes.erase(removeIt, childNodes.end());

    treeNode->addchildren(childNodes);

    for (auto& childNode : treeNode->getchildren()) {
        getnodedata(childNode);
        getnodes(childNode);
    }
}

void vkgltfmodel::getnodedata(std::shared_ptr<vknode> treeNode) {
    int nodeNum = treeNode->getnum();
    const tinygltf::Node& node = mmodel->nodes.at(nodeNum);
    treeNode->setname(node.name);

    if (node.translation.size()) {
        treeNode->settranslation(glm::make_vec3(node.translation.data()));
    }
    if (node.rotation.size()) {
        treeNode->setrotation(glm::make_quat(node.rotation.data()));
    }
    if (node.scale.size()) {
        treeNode->setscale(glm::make_vec3(node.scale.data()));
    }

    treeNode->calculatenodemat();
}

void vkgltfmodel::resetnodedata(std::shared_ptr<vknode> treeNode) {
    getnodedata(treeNode);

    for (auto& childNode : treeNode->getchildren()) {
        resetnodedata(childNode);
    }
}

std::vector<std::shared_ptr<vknode>> vkgltfmodel::getnodelist(
    std::vector<std::shared_ptr<vknode>>& nodeList, int nodeNum) {
    for (auto& childNode : nodeList.at(nodeNum)->getchildren()) {
        int childNodeNum = childNode->getnum();
        nodeList.at(childNodeNum) = childNode;
        getnodelist(nodeList, childNodeNum);
    }
    return nodeList;
}

std::vector<glm::mat4> vkgltfmodel::getinversebindmats() {
    return minversebindmats;
}

std::vector<unsigned int> vkgltfmodel::getnodetojoint() {
    return mnodetojoint;
}

















void vkgltfmodel::createvertexbuffers(vkobjs& objs) {
    mgltfobjs.rdgltfvertexbufferdata.reserve(mmodel->meshes[0].primitives.size());
    mgltfobjs.rdgltfvertexbufferdata.resize(mmodel->meshes[0].primitives.size());
    mattribaccs.reserve(mmodel->meshes[0].primitives.size());
    mattribaccs.resize(mmodel->meshes[0].primitives.size());
    for (size_t i{ 0 }; i < mmodel->meshes[0].primitives.size();i++) {
        const tinygltf::Primitive& prims = mmodel->meshes.at(0).primitives.at(i);
        mgltfobjs.rdgltfvertexbufferdata.at(i).reserve(prims.attributes.size());
        mgltfobjs.rdgltfvertexbufferdata.at(i).resize(prims.attributes.size());
        mattribaccs.at(i).reserve(prims.attributes.size());
        mattribaccs.at(i).resize(prims.attributes.size());
        for (const auto& a : prims.attributes) {
            const std::string atype = a.first;
            const int accnum = a.second;
            const tinygltf::Accessor& acc = mmodel->accessors.at(accnum);
            const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
            const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);
            if (atype.compare("POSITION") != 0 && atype.compare("NORMAL") != 0 && atype.compare("TEXCOORD_0") != 0 && atype.compare("JOINTS_0") && atype.compare("WEIGHTS_0") != 0) {
                continue;
            }


            mattribaccs.at(i).at(atts.at(atype)) = accnum;

            //mgltfobjs.rdgltfvertexbufferdata.at(atts.at(atype));
            //sizeof(acc.componentType);
            //size_t newbytelength{ bview.byteStride * acc.count };
            size_t newbytelength{ bview.byteLength };//bview.byteStride * acc.count };


            vkvbo::init(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(atts.at(atype)), newbytelength);
        }
    }
}


void vkgltfmodel::createindexbuffers(vkobjs& objs) {
    mgltfobjs.rdgltfindexbufferdata.reserve(mmodel->meshes[0].primitives.size());
    mgltfobjs.rdgltfindexbufferdata.resize(mmodel->meshes[0].primitives.size());
    for (size_t i{ 0 }; i < mmodel->meshes[0].primitives.size(); i++) {
        const tinygltf::Primitive& prims = mmodel->meshes.at(0).primitives.at(i);
        const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);
        const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
        const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);


        size_t newbytelength{ sizeof(unsigned short) * acc.count };

        indexbuffer::init(objs, mgltfobjs.rdgltfindexbufferdata.at(i), newbytelength);
    }
}


void vkgltfmodel::uploadvertexbuffers(vkobjs& objs){


    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); ++i) {
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); ++j) {
            const tinygltf::Accessor& acc = mmodel->accessors.at(mattribaccs.at(i).at(j));
            const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
            const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);

            vkvbo::upload(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(j), buff, bview, acc);
        }
    }

}

void vkgltfmodel::uploadindexbuffers(vkobjs& objs){

    for (size_t i{ 0 }; i < mmodel->meshes[0].primitives.size(); i++) {
        const tinygltf::Primitive& prims = mmodel->meshes.at(0).primitives.at(i);
        const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);
        const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
        const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);

        indexbuffer::upload(objs, mgltfobjs.rdgltfindexbufferdata.at(i), buff, bview, acc);
    }

}




int vkgltfmodel::gettricount(int i){
    const tinygltf::Primitive& prims = mmodel->meshes.at(0).primitives.at(i);
    const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);
    unsigned int c{ 0 };
    switch (prims.mode) {
    case TINYGLTF_MODE_TRIANGLES:
        c = acc.count / 3;
        break;
    default:
        c = 0;
        break;
    }

    return c;
}





void vkgltfmodel::draw(vkobjs& objs){

    //const tinygltf::Primitive& prims = mmodel->meshes.at(0).primitives.at(0);
    //const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);



    VkDeviceSize ofx{ 0 };
    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {
        vkCmdBindDescriptorSets(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objs.rdgltfpipelinelayout, 0, 1, &mgltfobjs.rdgltfmodeltex[i].texdescriptorset, 0, nullptr);
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            vkCmdBindVertexBuffers(objs.rdcommandbuffer, j, 1, &mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).rdvertexbuffer, &ofx);
        }
        vkCmdBindIndexBuffer(objs.rdcommandbuffer, mgltfobjs.rdgltfindexbufferdata.at(i).rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(objs.rdcommandbuffer, static_cast<uint32_t>(gettricount(i) * 3), 1, 0, 0, 0);
    }


}

void vkgltfmodel::drawinstanced(vkobjs& objs,VkPipelineLayout& vkplayout, int instancecount,int stride) {
    VkDeviceSize offset = 0;
    std::vector<vkpushconstants> pushes(mgltfobjs.rdgltfvertexbufferdata.size());


    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {
        pushes[i].pkmodelstride = stride;
        pushes[i].texidx = mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source;

        vkCmdPushConstants(objs.rdcommandbuffer, vkplayout,VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants), &pushes.at(i));


        vkCmdBindDescriptorSets(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.rdgltfmodeltex[0].texdescriptorset, 0, nullptr);
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            vkCmdBindVertexBuffers(objs.rdcommandbuffer, j, 1,&mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).rdvertexbuffer, &offset);
        }
        vkCmdBindIndexBuffer(objs.rdcommandbuffer, mgltfobjs.rdgltfindexbufferdata.at(i).rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);
        //ubo::upload(objs, objs.rdperspviewmatrixubo, mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source);
        vkCmdDrawIndexed(objs.rdcommandbuffer, static_cast<uint32_t>(gettricount(i) * 3), instancecount, 0, 0, 0);
    }

}












void vkgltfmodel::cleanup(vkobjs& objs){

    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            vkvbo::cleanup(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(j));
        }
    }
    for (int i{ 0 }; i < mgltfobjs.rdgltfindexbufferdata.size(); i++) {
        indexbuffer::cleanup(objs, mgltfobjs.rdgltfindexbufferdata.at(i));
    }
    for (int i{0};i<mgltfobjs.rdgltfmodeltex.size();i++)
    vktexture::cleanup(objs, mgltfobjs.rdgltfmodeltex[i]);

    mmodel.reset();

}


std::vector<vktexdata> vkgltfmodel::gettexdata() {
    return mgltfobjs.rdgltfmodeltex;
}