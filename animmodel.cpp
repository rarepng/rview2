
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>





#include "vkvbo.hpp"
#include "vkebo.hpp"
#include "animmodel.hpp"

bool animmodel::loadmodel(vkobjs& objs, std::string fname)
{

    //mmodel = std::make_shared<tinygltf::Model>();

    fastgltf::Parser fastparser{};
    auto buff = fastgltf::MappedGltfFile::FromPath(fname);
    auto a = fastparser.loadGltfBinary(buff.get(),"resources/");

    mmodel2 = std::move(a.get());

    //tinygltf::TinyGLTF loader;
    //std::string err;
    //std::string warr;
    //bool res = false;
    //res = loader.LoadBinaryFromFile(mmodel.get(), &err, &warr, fname);


    mgltfobjs.tex.reserve(mmodel2.images.size());
    mgltfobjs.tex.resize(mmodel2.images.size());
    if (!vktexture::loadtexture(objs, mgltfobjs.tex, mmodel2))return false;
    if (!vktexture::loadtexlayoutpool(objs, mgltfobjs.tex,mgltfobjs.texpls, mmodel2))return false;


    mmodelfilename = fname;


    //createvertexbuffers(objs);
    //createindexbuffers(objs);
    createvboebo(objs);

    getjointdata();
    //getweightdata();
    getinvbindmats();

    mjnodecount = mmodel2.nodes.size();

    getanims();

    return true;
}





int animmodel::getnodecount() {
    return mjnodecount;
}

gltfnodedata animmodel::getgltfnodes() {
    gltfnodedata nodeData{};

    int rootNodeNum = mmodel2.scenes.at(0).nodeIndices.at(0);

    nodeData.rootnode = vknode::createroot(rootNodeNum);

    getnodedata(nodeData.rootnode);
    getnodes(nodeData.rootnode);

    nodeData.nodelist.reserve(mjnodecount);
    nodeData.nodelist.resize(mjnodecount);
    nodeData.nodelist.at(rootNodeNum) = nodeData.rootnode;
    getnodelist(nodeData.nodelist, rootNodeNum);

    return nodeData;
}

void animmodel::getjointdata() {
    jointuintofx.reserve(mmodel2.meshes.size());
    jointuintofx.resize(mmodel2.meshes.size());
    for (size_t i{ 0 }; i < mmodel2.meshes.size(); i++) {
        for (size_t j{ 0 }; j < mmodel2.meshes[i].primitives.size(); j++) {
            const fastgltf::Accessor& accessor = mmodel2.accessors.at(mmodel2.meshes.at(i).primitives.at(j).findAttribute("JOINTS_0")->accessorIndex);
            const fastgltf::BufferView& bufferView = mmodel2.bufferViews.at(accessor.bufferViewIndex.value());
            const fastgltf::Buffer& buffer = mmodel2.buffers.at(bufferView.bufferIndex);
            //if (accessor.componentType == 5121) {
            //    jointzchar.reserve(accessor.count*4);
            //    jointzchar.resize(accessor.count*4);
            //    std::memcpy(jointzchar.data(), &buffer.data[bufferView.byteOffset + accessor.byteOffset], bufferView.byteLength);
            //    jointzint.insert(jointzint.end(), jointzchar.begin(), jointzchar.end());
            //    jointzchar.clear();
            //} else 
             if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                 if (i > 0)
                    jointuintofx.at(i) = jointzint.size();
                 else
                     jointuintofx.at(i) = 0;
                jointz.reserve(accessor.count * 4);
                jointz.resize(accessor.count * 4);
                std::visit(fastgltf::visitor{
                    [](auto& arg) {},
                    [&](const fastgltf::sources::Array& vector) {
                        std::memcpy(jointz.data(), vector.bytes.data() + bufferView.byteOffset + accessor.byteOffset, bufferView.byteLength);
                    } }, buffer.data);
                jointzint.insert(jointzint.end(), jointz.begin(), jointz.end());
                jointz.clear();
             } else {
                 if (i > 0)
                     jointuintofx.at(i) = jointuintofx.at(i - 1);
                 else
                     jointuintofx.at(i) = 0;
             }
        }
    }
    mnodetojoint.reserve(mmodel2.nodes.size());
    mnodetojoint.resize(mmodel2.nodes.size());

    const fastgltf::Skin& skin = mmodel2.skins.at(0);
        for (unsigned int i = 0; i < skin.joints.size(); ++i) {
            mnodetojoint.at(skin.joints.at(i)) = i;
        }
}


void animmodel::getinvbindmats() {
    //const tinygltf::Skin& skin = mmodel->skins.at(0);
    //int invBindMatAccessor = skin.inverseBindMatrices;

    //const tinygltf::Accessor& accessor = mmodel->accessors.at(invBindMatAccessor);
    //const tinygltf::BufferView& bufferView = mmodel->bufferViews.at(accessor.bufferView);
    //const tinygltf::Buffer& buffer = mmodel->buffers.at(bufferView.buffer);
     
     
    const fastgltf::Skin& skin = mmodel2.skins.at(0);
    size_t invBindMatAccessor = skin.inverseBindMatrices.value();

    const fastgltf::Accessor& accessor = mmodel2.accessors.at(invBindMatAccessor);
    const fastgltf::BufferView& bufferView = mmodel2.bufferViews.at(accessor.bufferViewIndex.value());
    const fastgltf::Buffer& buffer = mmodel2.buffers.at(bufferView.bufferIndex);

    minversebindmats.reserve(skin.joints.size());
    minversebindmats.resize(skin.joints.size());

    size_t newbytelength{ skin.joints.size() * 4 * 4 * 4 };
    std::visit(fastgltf::visitor{
        [](auto& arg) {},
        [&](const fastgltf::sources::Array& vector) {
            std::memcpy(minversebindmats.data(), vector.bytes.data()+bufferView.byteOffset + accessor.byteOffset, newbytelength);
        } }, buffer.data);


}

void animmodel::getanims() {
    manimclips.reserve(mmodel2.animations.size());
    //for (auto& anim0 : mmodel->animations) {
    //    std::shared_ptr<vkclip> clip0=std::make_shared<vkclip>(anim0.name);
    //    for (auto& c : anim0.channels) {
    //        clip0->addchan(mmodel, anim0, c);
    //    }
    //    manimclips.push_back(clip0);
    //}
    for (auto& anim0 : mmodel2.animations) {
        std::shared_ptr<vkclip> clip0=std::make_shared<vkclip>(static_cast<std::string>(anim0.name));
        for (auto& c : anim0.channels) {
            clip0->addchan(mmodel2, anim0, c);
        }
        manimclips.push_back(clip0);
    }
}

std::vector<std::shared_ptr<vkclip>> animmodel::getanimclips() {
    return manimclips;
}

void animmodel::getnodes(std::shared_ptr<vknode> treeNode) {
    int nodeNum = treeNode->getnum();
    const auto& childNodes = mmodel2.nodes.at(nodeNum).children;

    //auto removeIt = std::remove_if(childNodes.begin(), childNodes.end(),
    //    [&](size_t& num) { return mmodel2.nodes.at(num).skinIndex != -1; }
    //);
    //childNodes.erase(removeIt, childNodes.end());



    treeNode->addchildren(childNodes);

    for (auto& childNode : treeNode->getchildren()) {
        getnodedata(childNode);
        getnodes(childNode);
    }
}

void animmodel::getnodedata(std::shared_ptr<vknode> treeNode) {
    int nodeNum = treeNode->getnum();
    const fastgltf::Node& node = mmodel2.nodes.at(nodeNum);
    treeNode->setname(static_cast<std::string>(node.name));
    std::visit(fastgltf::visitor{
        [](auto& arg){},
        [&](fastgltf::TRS trs) {
            treeNode->settranslation(glm::make_vec3(trs.translation.data()));
            treeNode->setrotation(glm::make_quat(trs.rotation.value_ptr()));
            treeNode->setscale(glm::make_vec3(trs.scale.data()));
}
        }, node.transform);

    treeNode->calculatenodemat();
}

void animmodel::resetnodedata(std::shared_ptr<vknode> treeNode) {
    getnodedata(treeNode);

    for (auto& childNode : treeNode->getchildren()) {
        resetnodedata(childNode);
    }
}

std::vector<std::shared_ptr<vknode>> animmodel::getnodelist(std::vector<std::shared_ptr<vknode>>& nodeList, int nodeNum) {
    for (auto& childNode : nodeList.at(nodeNum)->getchildren()) {
        int childNodeNum = childNode->getnum();
        nodeList.at(childNodeNum) = childNode;
        getnodelist(nodeList, childNodeNum);
    }
    return nodeList;
}

std::vector<glm::mat4> animmodel::getinversebindmats() {
    return minversebindmats;
}

std::vector<unsigned int> animmodel::getnodetojoint() {
    return mnodetojoint;
}


void animmodel::createvboebo(vkobjs& objs){
    mgltfobjs.vbodata.reserve(mmodel2.meshes.size());
    mgltfobjs.vbodata.resize(mmodel2.meshes.size());
    mgltfobjs.ebodata.reserve(mmodel2.meshes.size());
    mgltfobjs.ebodata.resize(mmodel2.meshes.size());
    meshjointtype.reserve(mmodel2.meshes.size());
    meshjointtype.resize(mmodel2.meshes.size());
    for (size_t i{ 0 }; i < mmodel2.meshes.size(); i++) {
        mgltfobjs.ebodata[i].reserve(mmodel2.meshes[i].primitives.size());
        mgltfobjs.ebodata[i].resize(mmodel2.meshes[i].primitives.size());
        mgltfobjs.vbodata[i].reserve(mmodel2.meshes[i].primitives.size());
        mgltfobjs.vbodata[i].resize(mmodel2.meshes[i].primitives.size());
        for (auto it = mmodel2.meshes[i].primitives.begin(); it < mmodel2.meshes[i].primitives.end(); it++) {
            const auto& idx = std::distance(mmodel2.meshes[i].primitives.begin(), it);
            mgltfobjs.vbodata.at(i).at(idx).reserve(5);
            mgltfobjs.vbodata.at(i).at(idx).resize(5);
            const fastgltf::Accessor& idxacc = mmodel2.accessors[it->indicesAccessor.value()];
            const fastgltf::Accessor& posacc = mmodel2.accessors[it->findAttribute("POSITION")->accessorIndex];
            const fastgltf::Accessor& noracc = mmodel2.accessors[it->findAttribute("NORMAL")->accessorIndex];
            const fastgltf::Accessor& texacc = mmodel2.accessors[it->findAttribute("TEXCOORD_0")->accessorIndex];
            const fastgltf::Accessor& joiacc = mmodel2.accessors[it->findAttribute("JOINTS_0")->accessorIndex];
            const fastgltf::Accessor& weiacc = mmodel2.accessors[it->findAttribute("WEIGHTS_0")->accessorIndex];

            vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(0), posacc.count * 3 * 4);
            vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(1), noracc.count * 3 * 4);
            vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(2), texacc.count * 2 * 4);
            if (joiacc.componentType == fastgltf::ComponentType::UnsignedByte) {
                vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(3), joiacc.count * 4 * 1);
                meshjointtype.at(i) = false;
            }
            else {
                vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(3), joiacc.count * 4 * 4);
                meshjointtype.at(i) = true;
            }
            vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(4), weiacc.count * 4 * 4);

            vkebo::init(objs, mgltfobjs.ebodata.at(i).at(idx), idxacc.count * 2);
                
        }
    }
}



void animmodel::uploadvboebo(vkobjs& objs,VkCommandBuffer& cbuffer){
    for (size_t i{ 0 }; i < mmodel2.meshes.size(); i++) {
        for (auto it = mmodel2.meshes[i].primitives.begin(); it < mmodel2.meshes[i].primitives.end(); it++) {
            const auto& idx = std::distance(mmodel2.meshes[i].primitives.begin(), it);

            const fastgltf::Accessor& idxacc = mmodel2.accessors[it->indicesAccessor.value()];

            const fastgltf::Accessor& posacc = mmodel2.accessors[it->findAttribute("POSITION")->accessorIndex];
            const fastgltf::Accessor& noracc = mmodel2.accessors[it->findAttribute("NORMAL")->accessorIndex];
            const fastgltf::Accessor& texacc = mmodel2.accessors[it->findAttribute("TEXCOORD_0")->accessorIndex];
            const fastgltf::Accessor& joiacc = mmodel2.accessors[it->findAttribute("JOINTS_0")->accessorIndex];
            const fastgltf::Accessor& weiacc = mmodel2.accessors[it->findAttribute("WEIGHTS_0")->accessorIndex];

            const fastgltf::BufferView& idxbview = mmodel2.bufferViews[idxacc.bufferViewIndex.value()];

            const fastgltf::BufferView& posbview = mmodel2.bufferViews[posacc.bufferViewIndex.value()];
            const fastgltf::BufferView& norbview = mmodel2.bufferViews[noracc.bufferViewIndex.value()];
            const fastgltf::BufferView& texbview = mmodel2.bufferViews[texacc.bufferViewIndex.value()];
            const fastgltf::BufferView& joibview = mmodel2.bufferViews[joiacc.bufferViewIndex.value()];
            const fastgltf::BufferView& weibview = mmodel2.bufferViews[weiacc.bufferViewIndex.value()];

            const fastgltf::Buffer& b = mmodel2.buffers[0];

            vkebo::upload(objs,cbuffer, mgltfobjs.ebodata.at(i).at(idx), b, idxbview, idxacc.count);


            vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(0), b, posbview, posacc);
            vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(1), b, norbview, noracc);
            vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(2), b, texbview, texacc);

            if (joiacc.componentType == fastgltf::ComponentType::UnsignedShort) {
                vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(3), jointzint, joiacc.count, jointuintofx[i]);
            } else {
                vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(3), b, joibview, joiacc);
            }


            vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(4), b, weibview, weiacc);
            
        }
    }

}




int animmodel::gettricount(int i,int j){
    const fastgltf::Primitive& prims = mmodel2.meshes.at(i).primitives.at(j);
    const fastgltf::Accessor& acc = mmodel2.accessors.at(prims.indicesAccessor.value());
    unsigned int c{ 0 };
    //switch (prims.mode) {
    //case TINYGLTF_MODE_TRIANGLES:
    c = acc.count / 3;
    //    break;
    //default:
    //    c = 0;
    //    break;
    //}

    return c;
}





void animmodel::drawinstanced(vkobjs& objs,VkPipelineLayout& vkplayout, VkPipeline& vkpline, VkPipeline& vkplineuint, int instancecount,int stride) {
    VkDeviceSize offset = 0;
    std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.vbodata.size());

    vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.texpls.texdescriptorset, 0, nullptr);

    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {
        pushes[i].reserve(mgltfobjs.vbodata.at(i).size());
        pushes[i].resize(mgltfobjs.vbodata.at(i).size());
        meshjointtype[i] ? vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplineuint) : vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkpline);

        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {
            pushes[i][j].pkmodelstride = stride;
            pushes[i][j].texidx = static_cast<unsigned int>(mmodel2.textures[mmodel2.materials[mmodel2.meshes.at(i).primitives.at(j).materialIndex.value_or(0)].pbrData.baseColorTexture->textureIndex].imageIndex.value_or(0));
            pushes[i][j].t = (float)glfwGetTime();
            pushes[i][j].decaying = *objs.decaying;

            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants), &pushes.at(i).at(j));
            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer, &offset);
            }
            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.ebodata.at(i).at(j).bhandle, 0, VK_INDEX_TYPE_UINT16);
            //if(mmodel2.meshes.at(i).primitives.at(j).materialIndex.has_value())
            //if(mmodel2.textures[mmodel2.materials[mmodel2.meshes.at(i).primitives.at(j).materialIndex.value_or(0)].pbrData.baseColorTexture->textureIndex].imageIndex.has_value())
                vkCmdDrawIndexed(objs.rdcommandbuffer[0], static_cast<uint32_t>(gettricount(i, j) * 3), instancecount, 0, 0, 0);
            //else {
            //    vkCmdDrawIndexed(objs.rdcommandbuffer[0], 0, instancecount, 0, 0, 0);
            //}
        }
    }

}

void animmodel::drawinstanced(vkobjs& objs, VkPipelineLayout& vkplayout, int instancecount, int stride, double& decaytime, bool* decaying){
    VkDeviceSize offset = 0;
    std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.vbodata.size());
    if (decaytime > 0.8)*decaying = false;
    vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.texpls.texdescriptorset, 0, nullptr);

    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {
        pushes[i].reserve(mgltfobjs.vbodata.at(i).size());
        pushes[i].resize(mgltfobjs.vbodata.at(i).size());

        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {
            pushes[i][j].pkmodelstride = stride;
            pushes[i][j].texidx = 0;
            pushes[i][j].t = decaytime;

            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants), &pushes.at(i).at(j));
            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer, &offset);
            }
            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.ebodata.at(i).at(j).bhandle, 0, VK_INDEX_TYPE_UINT16);
            //ubo::upload(objs, objs.rdperspviewmatrixubo, mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source);
            vkCmdDrawIndexed(objs.rdcommandbuffer[0], static_cast<uint32_t>(gettricount(i, j) * 3), instancecount, 0, 0, 0);
        }
    }
}




void animmodel::cleanup(vkobjs& objs){

    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {
        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {
            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
                vkvbo::cleanup(objs, mgltfobjs.vbodata.at(i).at(j).at(k));
            }
        }
    }
    for (int i{ 0 }; i < mgltfobjs.ebodata.size(); i++) {
        for (int j{ 0 }; j < mgltfobjs.ebodata.at(i).size(); j++) {
            vkebo::cleanup(objs, mgltfobjs.ebodata.at(i).at(j));
        }
    }
    for (int i{ 0 }; i < mgltfobjs.tex.size(); i++) {
        vktexture::cleanup(objs, mgltfobjs.tex[i]);
    }
    vktexture::cleanuppls(objs, mgltfobjs.texpls);

    //mmodel.reset();

}


std::vector<vktexdata> animmodel::gettexdata() {
    return mgltfobjs.tex;
}

vktexdatapls animmodel::gettexdatapls(){
    return mgltfobjs.texpls;
}
