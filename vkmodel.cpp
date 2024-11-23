//#define GLM_ENABLE_EXPERIMENTAL
//#include <algorithm>
//#include <chrono>
//#include <cmath>
//#include <glm/gtc/type_ptr.hpp>
//#include <glm/gtx/quaternion.hpp>
//#include <glm/gtx/dual_quaternion.hpp>
//#include <glm/gtx/matrix_decompose.hpp>
//
//#include "vkvbo.hpp"
//#include "vkebo.hpp"
//#include "vkmodel.hpp"
//
//vkmodel::vkmodel(bool gltf, bool skeletonanim, bool texturefile, bool screen){
//	mmbools.mgltf = gltf;
//	mmbools.mskeletonanim = skeletonanim;
//	mmbools.mtexturefile = texturefile;
//	mmbools.mscreen = screen;
//}
//
//bool vkmodel::loadmodel(vkobjs& objs, std::vector<std::string> fname){
//	if (mmbools.mgltf) {
//        mmodel = std::make_shared<tinygltf::Model>();
//
//        tinygltf::TinyGLTF loader;
//        std::string err;
//        std::string warr;
//        bool res = false;
//        res = loader.LoadBinaryFromFile(mmodel.get(), &err, &warr, fname[0]);
//        mgltfobjs.tex.reserve(mmodel->images.size());
//        mgltfobjs.tex.resize(mmodel->images.size());
//        if (!vktexture::loadtexture(objs, mgltfobjs.tex, mmodel))return false;
//        if (!vktexture::loadtexlayoutpool(objs, mgltfobjs.tex, mgltfobjs.texpls, mmodel))return false;
//
//        if (mmbools.mskeletonanim) {
//
//            getjointdata();
//            getinvbindmats();
//
//            mjnodecount = mmodel->nodes.size();
//
//            getanims();
//        }
//
//	}
//    if (mmbools.mtexturefile) {
//        mgltfobjs.tex.reserve(fname.size());
//        mgltfobjs.tex.resize(fname.size());
//        for(size_t i{0};i<fname.size();i++)
//        vktexture::loadtexturefile(objs, mgltfobjs.tex[i], mgltfobjs.texpls, fname[i]);
//    }
//
//
//    createvertexbuffers(objs);
//    createindexbuffers(objs);
//
//	return true;
//}
//int vkmodel::getnodecount() {
//    return mjnodecount;
//}
//
//gltfnodedata vkmodel::getgltfnodes() {
//    gltfnodedata nodeData{};
//
//    int rootNodeNum = mmodel->scenes.at(0).nodes.at(0);
//
//    nodeData.rootnode = vknode::createroot(rootNodeNum);
//
//    getnodedata(nodeData.rootnode);
//    getnodes(nodeData.rootnode);
//
//    nodeData.nodelist.reserve(mjnodecount);
//    nodeData.nodelist.resize(mjnodecount);
//    nodeData.nodelist.at(rootNodeNum) = nodeData.rootnode;
//    getnodelist(nodeData.nodelist, rootNodeNum);
//
//    return nodeData;
//}
//
//void vkmodel::getjointdata() {
//    //for (size_t i{ 0 }; i < mmodel->meshes.size(); i++) {
//    //    for (size_t j{ 0 }; j < mmodel->meshes[i].primitives.size(); j++) {
//    //        const tinygltf::Accessor& accessor = mmodel->accessors.at(mmodel->meshes.at(i).primitives.at(j).attributes.at("JOINTS_0"));
//    //        const tinygltf::BufferView& bufferView = mmodel->bufferViews.at(accessor.bufferView);
//    //        const tinygltf::Buffer& buffer = mmodel->buffers.at(bufferView.buffer);
//    //        if (accessor.componentType == 5121) {
//    //            jointzchar.reserve(accessor.count*4);
//    //            jointzchar.resize(accessor.count*4);
//    //            std::memcpy(jointzchar.data(), &buffer.data[bufferView.byteOffset + accessor.byteOffset], bufferView.byteLength);
//    //            jointzint.insert(jointzint.end(), jointzchar.begin(), jointzchar.end());
//    //            jointzchar.clear();
//    //        } else if (accessor.componentType == 5123) {
//    //            jointz.reserve(accessor.count * 4);
//    //            jointz.resize(accessor.count * 4);
//    //            std::memcpy(jointz.data(), &buffer.data[bufferView.byteOffset + accessor.byteOffset], bufferView.byteLength);
//    //            jointzint.insert(jointzint.end(), jointz.begin(), jointz.end());
//    //            jointz.clear();
//    //        } else {
//    //            size_t jend{ jointzint.size() };
//    //            jointzint.reserve(jointzint.size() + accessor.count * 4);
//    //            jointzint.resize(jointzint.size() + accessor.count * 4);
//    //            std::memcpy(&jointzint.at(jend), &buffer.data[bufferView.byteOffset + accessor.byteOffset], bufferView.byteLength);
//    //        }
//    //    }
//    //}
//    mnodetojoint.reserve(mmodel->nodes.size());
//    mnodetojoint.resize(mmodel->nodes.size());
//
//    const tinygltf::Skin& skin = mmodel->skins.at(0);
//    for (unsigned int i = 0; i < skin.joints.size(); ++i) {
//        mnodetojoint.at(skin.joints.at(i)) = i;
//    }
//}
//
//
//void vkmodel::getinvbindmats() {
//    const tinygltf::Skin& skin = mmodel->skins.at(0);
//    int invBindMatAccessor = skin.inverseBindMatrices;
//
//    const tinygltf::Accessor& accessor = mmodel->accessors.at(invBindMatAccessor);
//    const tinygltf::BufferView& bufferView = mmodel->bufferViews.at(accessor.bufferView);
//    const tinygltf::Buffer& buffer = mmodel->buffers.at(bufferView.buffer);
//
//    minversebindmats.reserve(skin.joints.size());
//    minversebindmats.resize(skin.joints.size());
//
//    size_t newbytelength{ skin.joints.size() * 4 * 4 * 4 };
//
//    std::memcpy(minversebindmats.data(), &buffer.data[bufferView.byteOffset + accessor.byteOffset], newbytelength);
//}
//
//void vkmodel::getanims() {
//    manimclips.reserve(mmodel->animations.size());
//    for (auto& anim0 : mmodel->animations) {
//        std::shared_ptr<vkclip> clip0 = std::make_shared<vkclip>(anim0.name);
//        for (auto& c : anim0.channels) {
//            clip0->addchan(mmodel, anim0, c);
//        }
//        manimclips.push_back(clip0);
//    }
//}
//
//std::vector<std::shared_ptr<vkclip>> vkmodel::getanimclips() {
//    return manimclips;
//}
//
//void vkmodel::getnodes(std::shared_ptr<vknode> treeNode) {
//    int nodeNum = treeNode->getnum();
//    std::vector<int> childNodes = mmodel->nodes.at(nodeNum).children;
//
//    /* remove the child node with skin/mesh metadata */
//    auto removeIt = std::remove_if(childNodes.begin(), childNodes.end(),
//        [&](int num) { return mmodel->nodes.at(num).skin != -1; }
//    );
//    childNodes.erase(removeIt, childNodes.end());
//
//    treeNode->addchildren(childNodes);
//
//    for (auto& childNode : treeNode->getchildren()) {
//        getnodedata(childNode);
//        getnodes(childNode);
//    }
//}
//
//void vkmodel::getnodedata(std::shared_ptr<vknode> treeNode) {
//    int nodeNum = treeNode->getnum();
//    const tinygltf::Node& node = mmodel->nodes.at(nodeNum);
//    treeNode->setname(node.name);
//
//    if (node.translation.size()) {
//        treeNode->settranslation(glm::make_vec3(node.translation.data()));
//    }
//    if (node.rotation.size()) {
//        treeNode->setrotation(glm::make_quat(node.rotation.data()));
//    }
//    if (node.scale.size()) {
//        treeNode->setscale(glm::make_vec3(node.scale.data()));
//    }
//
//    treeNode->calculatenodemat();
//}
//
//void vkmodel::resetnodedata(std::shared_ptr<vknode> treeNode) {
//    getnodedata(treeNode);
//
//    for (auto& childNode : treeNode->getchildren()) {
//        resetnodedata(childNode);
//    }
//}
//
//std::vector<std::shared_ptr<vknode>> vkmodel::getnodelist(std::vector<std::shared_ptr<vknode>>& nodeList, int nodeNum) {
//    for (auto& childNode : nodeList.at(nodeNum)->getchildren()) {
//        int childNodeNum = childNode->getnum();
//        nodeList.at(childNodeNum) = childNode;
//        getnodelist(nodeList, childNodeNum);
//    }
//    return nodeList;
//}
//
//std::vector<glm::mat4> vkmodel::getinversebindmats() {
//    return minversebindmats;
//}
//
//std::vector<unsigned int> vkmodel::getnodetojoint() {
//    return mnodetojoint;
//}
//
//
//
//
//
//void vkmodel::createvertexbuffers(vkobjs& objs) {
//    if ( mmbools.mskeletonanim) {
//        mgltfobjs.vbodata.reserve(mmodel->meshes.size());
//        mgltfobjs.vbodata.resize(mmodel->meshes.size());
//        mattribaccs.reserve(mmodel->meshes.size());
//        mattribaccs.resize(mmodel->meshes.size());
//        for (size_t i{ 0 }; i < mmodel->meshes.size(); i++) {
//            mgltfobjs.vbodata[i].reserve(mmodel->meshes[i].primitives.size());
//            mgltfobjs.vbodata[i].resize(mmodel->meshes[i].primitives.size());
//            mattribaccs[i].reserve(mmodel->meshes[i].primitives.size());
//            mattribaccs[i].resize(mmodel->meshes[i].primitives.size());
//            for (size_t j{ 0 }; j < mmodel->meshes[i].primitives.size(); j++) {
//                const tinygltf::Primitive& prims = mmodel->meshes.at(i).primitives.at(j);
//                mgltfobjs.vbodata.at(i).at(j).reserve(prims.attributes.size());
//                mgltfobjs.vbodata.at(i).at(j).resize(prims.attributes.size());
//                mattribaccs.at(i).at(j).reserve(prims.attributes.size());
//                mattribaccs.at(i).at(j).resize(prims.attributes.size());
//                for (const auto& a : prims.attributes) {
//                    const std::string atype = a.first;
//                    const int accnum = a.second;
//                    const tinygltf::Accessor& acc = mmodel->accessors.at(accnum);
//                    const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
//                    const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);
//                    if (atype.compare("POSITION") != 0 && atype.compare("NORMAL") != 0 && atype.compare("TEXCOORD_0") != 0 && atype.compare("JOINTS_0") && atype.compare("WEIGHTS_0") != 0) {
//                        mattribaccs.at(i).at(j).pop_back();
//                        mgltfobjs.vbodata.at(i).at(j).pop_back();
//                        continue;
//                    }
//
//
//                    mattribaccs.at(i).at(j).at(atts.at(atype)) = accnum;
//
//                    //mgltfobjs.vbodata.at(atts.at(atype));
//                    //sizeof(acc.componentType);
//                    //size_t newbytelength{ bview.byteStride * acc.count };
//                    size_t newbytelength{ bview.byteLength };//bview.byteStride * acc.count };
//
//                    //if (atype.compare("JOINTS_0") == 0) {
//                    //    if (acc.componentType == 5121) {
//                    //        newbytelength *= 4;
//                    //    }
//                    //    if (acc.componentType == 5123) {
//                    //        newbytelength *= 2;
//                    //    }
//                    //}
//
//
//                    vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(j).at(atts.at(atype)), newbytelength);
//                }
//            }
//        }
//    } 
//    else if (mmbools.mgltf) {
//        mgltfobjs.vbodata.reserve(mmodel->meshes.size());
//        mgltfobjs.vbodata.resize(mmodel->meshes.size());
//        mattribaccs.reserve(mmodel->meshes.size());
//        mattribaccs.resize(mmodel->meshes.size());
//        for (size_t i{ 0 }; i < mmodel->meshes.size(); i++) {
//            mgltfobjs.vbodata.at(i).reserve(mmodel->meshes[i].primitives.size());
//            mgltfobjs.vbodata.at(i).resize(mmodel->meshes[i].primitives.size());
//            mattribaccs.at(i).reserve(mmodel->meshes[i].primitives.size());
//            mattribaccs.at(i).resize(mmodel->meshes[i].primitives.size());
//            for (size_t j{ 0 }; j < mmodel->meshes[i].primitives.size(); j++) {
//                const tinygltf::Primitive& prims = mmodel->meshes.at(i).primitives.at(j);
//                mgltfobjs.vbodata.at(i).at(j).reserve(prims.attributes.size());
//                mgltfobjs.vbodata.at(i).at(j).resize(prims.attributes.size());
//                mattribaccs.at(i).at(j).reserve(prims.attributes.size());
//                mattribaccs.at(i).at(j).resize(prims.attributes.size());
//                for (const auto& a : prims.attributes) {
//                    const std::string atype = a.first;
//                    const int accnum = a.second;
//                    const tinygltf::Accessor& acc = mmodel->accessors.at(accnum);
//                    const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
//                    const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);
//                    if (atype.compare("POSITION") != 0 && atype.compare("NORMAL") != 0 && atype.compare("TEXCOORD_0") != 0) {
//                        mattribaccs.at(i).at(j).pop_back();
//                        mgltfobjs.vbodata.at(i).at(j).pop_back();
//                        continue;
//                    }
//
//
//                    mattribaccs.at(i).at(j).at(atts.at(atype)) = accnum;
//
//                    //mgltfobjs.vbodata.at(atts.at(atype));
//                    //sizeof(acc.componentType);
//                    //size_t newbytelength{ bview.byteStride * acc.count };
//                    size_t newbytelength{ bview.byteLength };//bview.byteStride * acc.count };
//
//
//                    vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(j).at(atts.at(atype)), newbytelength);
//                }
//            }
//        }
//    }
//    else if (mmbools.mtexturefile) {
//        mgltfobjs.vbodata.reserve(1);
//        mgltfobjs.vbodata.resize(1);
//        mgltfobjs.vbodata.at(0).reserve(1);
//        mgltfobjs.vbodata.at(0).resize(1);
//        mgltfobjs.vbodata.at(0).at(0).reserve(2);
//        mgltfobjs.vbodata.at(0).at(0).resize(2);
//        vkvbo::init(objs, mgltfobjs.vbodata.at(0).at(0).at(0), mquad.size() * sizeof(glm::vec3));
//        vkvbo::init(objs, mgltfobjs.vbodata.at(0).at(0).at(1), mquad.size() * sizeof(glm::vec2));
//    }
//    else {
//        mgltfobjs.vbodata.reserve(1);
//        mgltfobjs.vbodata.resize(1);
//        mgltfobjs.vbodata.at(0).reserve(1);
//        mgltfobjs.vbodata.at(0).resize(1);
//        mgltfobjs.vbodata.at(0).at(0).reserve(1);
//        mgltfobjs.vbodata.at(0).at(0).resize(1);
//        vkvbo::init(objs, mgltfobjs.vbodata.at(0).at(0).at(0), mquad.size() * sizeof(glm::vec3));
//    }
//}
//
//
//void vkmodel::createindexbuffers(vkobjs& objs) {
//    if (mmbools.mgltf) {
//        mgltfobjs.ebodata.reserve(mmodel->meshes.size());
//        mgltfobjs.ebodata.resize(mmodel->meshes.size());
//        for (size_t i{ 0 }; i < mmodel->meshes.size(); i++) {
//            mgltfobjs.ebodata.at(i).reserve(mmodel->meshes[i].primitives.size());
//            mgltfobjs.ebodata.at(i).resize(mmodel->meshes[i].primitives.size());
//            for (size_t j{ 0 }; j < mmodel->meshes[i].primitives.size(); j++) {
//                const tinygltf::Primitive& prims = mmodel->meshes.at(i).primitives.at(j);
//                const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);
//                const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
//                const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);
//
//
//                size_t newbytelength{ sizeof(unsigned short) * acc.count };
//
//                vkebo::init(objs, mgltfobjs.ebodata.at(i).at(j), newbytelength);
//            }
//        }
//    }
//    else {
//        mgltfobjs.ebodata.reserve(1);
//        mgltfobjs.ebodata.resize(1);
//        mgltfobjs.ebodata.at(0).reserve(1);
//        mgltfobjs.ebodata.at(0).resize(1);
//        vkebo::init(objs, mgltfobjs.ebodata.at(0).at(0), mindices.size() * sizeof(unsigned short));
//    }
//}
//
//
//void vkmodel::uploadvertexbuffers(vkobjs& objs) {
//
//    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); ++i) {
//        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); ++j) {
//            if (mmbools.mgltf) {
//                for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); ++k) {
//                    const tinygltf::Accessor& acc = mmodel->accessors.at(mattribaccs.at(i).at(j).at(k));
//                    const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
//                    const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);
//                    vkvbo::upload(objs, mgltfobjs.vbodata.at(i).at(j).at(k), buff, bview, acc);
//                }
//            } else {
//                vkvbo::upload(objs, mgltfobjs.vbodata.at(i).at(j).at(0), mquad);
//                if(mmbools.mtexturefile)
//                vkvbo::upload(objs, mgltfobjs.vbodata.at(i).at(j).at(1), mtexcoords);
//            }
//        }
//    }
//
//}
//
//void vkmodel::uploadindexbuffers(vkobjs& objs) {
//
//    for (size_t i{ 0 }; i < mgltfobjs.ebodata.size(); i++) {
//        for (size_t j{ 0 }; j < mgltfobjs.ebodata.at(i).size(); j++) {
//            if (mmbools.mgltf) {
//                const tinygltf::Primitive& prims = mmodel->meshes.at(i).primitives.at(j);
//                const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);
//                const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
//                const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);
//
//                vkebo::upload(objs, mgltfobjs.ebodata.at(i).at(j), buff, bview, acc);
//            } else {
//                vkebo::upload(objs, mgltfobjs.ebodata.at(0).at(0), mindices);
//            }
//        }
//    }
//
//}
//
//
//int vkmodel::gettricount(int i, int j) {
//    if (mmbools.mgltf) {
//        const tinygltf::Primitive& prims = mmodel->meshes.at(i).primitives.at(j);
//        const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);
//        unsigned int c{ 0 };
//        switch (prims.mode) {
//        case TINYGLTF_MODE_TRIANGLES:
//            c = acc.count / 3;
//            break;
//        default:
//            c = 0;
//            break;
//        }
//
//        return c;
//    } else {
//        return 2;
//    }
//}
//
//
//
//
//void vkmodel::cleanup(vkobjs& objs) {
//
//    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {
//        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {
//            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
//                vkvbo::cleanup(objs, mgltfobjs.vbodata.at(i).at(j).at(k));
//            }
//        }
//    }
//    for (int i{ 0 }; i < mgltfobjs.ebodata.size(); i++) {
//        for (int j{ 0 }; j < mgltfobjs.ebodata.at(i).size(); j++) {
//            vkebo::cleanup(objs, mgltfobjs.ebodata.at(i).at(j));
//        }
//    }
//    for (int i{ 0 }; i < mgltfobjs.tex.size(); i++) {
//        vktexture::cleanup(objs, mgltfobjs.tex[i]);
//    }
//    vktexture::cleanuppls(objs, mgltfobjs.texpls);
//
//    mmodel.reset();
//
//}
//
//
//std::vector<vktexdata> vkmodel::gettexdata() {
//    return mgltfobjs.tex;
//}
//
//vktexdatapls vkmodel::gettexdatapls() {
//    return mgltfobjs.texpls;
//}
//
//
//
//
//void vkmodel::draw(vkobjs& objs, VkPipelineLayout& vkplayout, double& time, double& time2, double& life) {
//    VkDeviceSize offset = 0;
//    if(mmbools.mtexturefile)
//    vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.texpls.texdescriptorset, 0, nullptr);
//    double pushes[] = { time,time2,life };
//    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {
//
//        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {
//
//            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 3 * sizeof(double), &pushes);
//
//
//            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
//                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer, &offset);
//            }
//            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.ebodata.at(i).at(j).bhandle, 0, VK_INDEX_TYPE_UINT16);
//            vkCmdDrawIndexed(objs.rdcommandbuffer[0], 6, 1, 0, 0, 0);
//        }
//    }
//
//}
//
//
//
//void vkmodel::drawgltf(vkobjs& objs, VkPipelineLayout& vkplayout, int instancecount, int stride) {
//    VkDeviceSize offset = 0;
//    std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.vbodata.size());
//
//    vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.texpls.texdescriptorset, 0, nullptr);
//
//    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {
//        pushes[i].reserve(mgltfobjs.vbodata.at(i).size());
//        pushes[i].resize(mgltfobjs.vbodata.at(i).size());
//
//        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {
//            pushes[i][j].pkmodelstride = stride;
//            pushes[i][j].texidx = mmodel->textures[mmodel->materials[mmodel->meshes.at(i).primitives.at(j).material].pbrMetallicRoughness.baseColorTexture.index].source;
//            pushes[i][j].t = (float)glfwGetTime();
//            pushes[i][j].decaying = *objs.decaying;
//
//            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants), &pushes.at(i).at(j));
//            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
//                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer, &offset);
//            }
//            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.ebodata.at(i).at(j).bhandle, 0, VK_INDEX_TYPE_UINT16);
//            //ubo::upload(objs, objs.rdperspviewmatrixubo, mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source);
//            vkCmdDrawIndexed(objs.rdcommandbuffer[0], static_cast<uint32_t>(gettricount(i, j) * 3), instancecount, 0, 0, 0);
//        }
//    }
//
//}
//
//void vkmodel::drawdecay(vkobjs& objs, VkPipelineLayout& vkplayout, int instancecount, int stride, double& decaytime, bool* decaying) {
//    VkDeviceSize offset = 0;
//    std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.vbodata.size());
//    if (decaytime > 0.8)*decaying = false;
//    vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.texpls.texdescriptorset, 0, nullptr);
//
//    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {
//        pushes[i].reserve(mgltfobjs.vbodata.at(i).size());
//        pushes[i].resize(mgltfobjs.vbodata.at(i).size());
//
//        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {
//            pushes[i][j].pkmodelstride = stride;
//            pushes[i][j].texidx = mmodel->textures[mmodel->materials[mmodel->meshes.at(i).primitives.at(j).material].pbrMetallicRoughness.baseColorTexture.index].source;
//            pushes[i][j].t = decaytime;
//
//            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants), &pushes.at(i).at(j));
//            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
//                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer, &offset);
//            }
//            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.ebodata.at(i).at(j).bhandle, 0, VK_INDEX_TYPE_UINT16);
//            //ubo::upload(objs, objs.rdperspviewmatrixubo, mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source);
//            vkCmdDrawIndexed(objs.rdcommandbuffer[0], static_cast<uint32_t>(gettricount(i, j) * 3), instancecount, 0, 0, 0);
//        }
//    }
//}
//
//
