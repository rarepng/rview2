
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
#include "vkgltfstatic.hpp"

bool vkgltfstatic::loadmodel(vkobjs& objs, std::string fname)
{


    mmodel = std::make_shared<tinygltf::Model>();

    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warr;
    bool res = false;
    if(!loader.LoadBinaryFromFile(mmodel.get(), &err, &warr, fname))return false;
    mgltfobjs.rdgltfmodeltex.reserve(mmodel->images.size());
    mgltfobjs.rdgltfmodeltex.resize(mmodel->images.size());
    if (!vktexture::loadtexture(objs, mgltfobjs.rdgltfmodeltex, mmodel))return false;
    if (!vktexture::loadtexlayoutpool(objs, mgltfobjs.rdgltfmodeltex, mmodel))return false;


    mmodelfilename = fname;


    createvertexbuffers(objs);
    createindexbuffers(objs);

    return true;
}



std::string vkgltfstatic::getmodelfname() {
    return mmodelfilename;
}



void vkgltfstatic::createvertexbuffers(vkobjs& objs) {
    mgltfobjs.rdgltfvertexbufferdata.reserve(mmodel->meshes[0].primitives.size());
    mgltfobjs.rdgltfvertexbufferdata.resize(mmodel->meshes[0].primitives.size());
    mattribaccs.reserve(mmodel->meshes[0].primitives.size());
    mattribaccs.resize(mmodel->meshes[0].primitives.size());
    for (size_t i{ 0 }; i < mmodel->meshes[0].primitives.size(); i++) {
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
            if (atype.compare("POSITION") != 0 && atype.compare("NORMAL") != 0 && atype.compare("TEXCOORD_0")) {
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


void vkgltfstatic::createindexbuffers(vkobjs& objs) {
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


void vkgltfstatic::uploadvertexbuffers(vkobjs& objs) {


    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); ++i) {
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); ++j) {
            const tinygltf::Accessor& acc = mmodel->accessors.at(mattribaccs.at(i).at(j));
            const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
            const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);

            vkvbo::upload(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(j), buff, bview, acc);
        }
    }

}

void vkgltfstatic::uploadindexbuffers(vkobjs& objs) {

    for (size_t i{ 0 }; i < mmodel->meshes[0].primitives.size(); i++) {
        const tinygltf::Primitive& prims = mmodel->meshes.at(0).primitives.at(i);
        const tinygltf::Accessor& acc = mmodel->accessors.at(prims.indices);
        const tinygltf::BufferView& bview = mmodel->bufferViews.at(acc.bufferView);
        const tinygltf::Buffer& buff = mmodel->buffers.at(bview.buffer);

        indexbuffer::upload(objs, mgltfobjs.rdgltfindexbufferdata.at(i), buff, bview, acc);
    }

}


int vkgltfstatic::gettricount(int i) {
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


void vkgltfstatic::draw(vkobjs& objs) {

    VkDeviceSize ofx{ 0 };
    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {
        vkCmdBindDescriptorSets(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objs.rdgltfpipelinelayout, 0, 1, &mgltfobjs.rdgltfmodeltex[i].texdescriptorset, 0, nullptr);
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            vkCmdBindVertexBuffers(objs.rdcommandbuffer, j, 1, &mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).rdvertexbuffer, &ofx);
        }
        vkCmdBindIndexBuffer(objs.rdcommandbuffer, mgltfobjs.rdgltfindexbufferdata.at(i).rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(objs.rdcommandbuffer, static_cast<uint32_t>(gettricount(i) * 3), 1, 0, 0, 0);
    }










    //vkCmdBindDescriptorSets(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objs.rdgltfpipelinelayout, 0, 1, &mgltfobjs.rdgltfmodeltex.texdescriptorset, 0, nullptr);

    //VkDeviceSize ofx{ 0 };
    //for (int i{ 0 }; i < 3; ++i) {
    //    vkCmdBindVertexBuffers(objs.rdcommandbuffer, i, 1, &gltfobjs.rdgltfvertexbufferdata.at(i).rdvertexbuffer, &ofx);
    //}

    //vkCmdBindIndexBuffer(objs.rdcommandbuffer, gltfobjs.rdgltfindexbufferdata.rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);

    //vkCmdBindPipeline(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objs.rdgltfpipeline);

    //vkCmdDrawIndexed(objs.rdcommandbuffer, static_cast<uint32_t>(objs.rdgltftricount * 3), 1, 0, 0, 0);


}

void vkgltfstatic::drawinstanced(vkobjs& objs,VkPipelineLayout& vkplayout, int instancecount, int stride) {
    VkDeviceSize offset = 0;
    std::vector<vkpushconstants> pushes(mgltfobjs.rdgltfvertexbufferdata.size());


    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {
        pushes[i].pkmodelstride = stride;
        pushes[i].texidx = mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source;

        vkCmdPushConstants(objs.rdcommandbuffer, vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants), &pushes.at(i));


        vkCmdBindDescriptorSets(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.rdgltfmodeltex[0].texdescriptorset, 0, nullptr);
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            vkCmdBindVertexBuffers(objs.rdcommandbuffer, j, 1, &mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).rdvertexbuffer, &offset);
        }
        vkCmdBindIndexBuffer(objs.rdcommandbuffer, mgltfobjs.rdgltfindexbufferdata.at(i).rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);
        //ubo::upload(objs, objs.rdperspviewmatrixubo, mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source);
        vkCmdDrawIndexed(objs.rdcommandbuffer, static_cast<uint32_t>(gettricount(i) * 3), instancecount, 0, 0, 0);
    }

}

void vkgltfstatic::cleanup(vkobjs& objs) {

    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            vkvbo::cleanup(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(j));
        }
    }
    for (int i{ 0 }; i < mgltfobjs.rdgltfindexbufferdata.size(); i++) {
        indexbuffer::cleanup(objs, mgltfobjs.rdgltfindexbufferdata.at(i));
    }
    for (int i{ 0 }; i < mgltfobjs.rdgltfmodeltex.size(); i++)
        vktexture::cleanup(objs, mgltfobjs.rdgltfmodeltex[i]);

    mmodel.reset();

}


std::vector<vktexdata> vkgltfstatic::gettexdata() {
    return mgltfobjs.rdgltfmodeltex;
}