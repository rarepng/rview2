
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <chrono>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "vkvbo.hpp"
#include "vkebo.hpp"
#include "staticmodel.hpp"

bool staticmodel::loadmodel(vkobjs& objs, std::string fname)
{



    fastgltf::Parser fastparser{};
    auto buff = fastgltf::MappedGltfFile::FromPath(fname);
    auto a = fastparser.loadGltfBinary(buff.get(), "resources/");

    mmodel = std::move(a.get());




    mgltfobjs.tex.reserve(mmodel.images.size());
    mgltfobjs.tex.resize(mmodel.images.size());
    if (!vktexture::loadtexture(objs, mgltfobjs.tex, mmodel))return false;
    if (!vktexture::loadtexlayoutpool(objs, mgltfobjs.tex,mgltfobjs.texpls, mmodel))return false;


    mmodelfilename = fname;


    createvboebo(objs);

    return true;
}


void staticmodel::createvboebo(vkobjs& objs) {
    mgltfobjs.rdgltfvertexbufferdata.reserve(mmodel.meshes.size());
    mgltfobjs.rdgltfvertexbufferdata.resize(mmodel.meshes.size());
    mgltfobjs.rdgltfindexbufferdata.reserve(mmodel.meshes.size());
    mgltfobjs.rdgltfindexbufferdata.resize(mmodel.meshes.size());
    for (size_t i{ 0 }; i < mmodel.meshes.size(); i++) {
        mgltfobjs.rdgltfindexbufferdata[i].reserve(mmodel.meshes[i].primitives.size());
        mgltfobjs.rdgltfindexbufferdata[i].resize(mmodel.meshes[i].primitives.size());
        mgltfobjs.rdgltfvertexbufferdata[i].reserve(mmodel.meshes[i].primitives.size());
        mgltfobjs.rdgltfvertexbufferdata[i].resize(mmodel.meshes[i].primitives.size());
        for (auto it = mmodel.meshes[i].primitives.begin(); it < mmodel.meshes[i].primitives.end(); it++) {
            const auto& idx = std::distance(mmodel.meshes[i].primitives.begin(), it);
            mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).reserve(3);
            mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).resize(3);
            const fastgltf::Accessor& idxacc = mmodel.accessors[it->indicesAccessor.value()];
            const fastgltf::Accessor& posacc = mmodel.accessors[it->findAttribute("POSITION")->accessorIndex];
            const fastgltf::Accessor& noracc = mmodel.accessors[it->findAttribute("NORMAL")->accessorIndex];
            const fastgltf::Accessor& texacc = mmodel.accessors[it->findAttribute("TEXCOORD_0")->accessorIndex];

            vkvbo::init(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).at(0), posacc.count * 3 * 4);
            vkvbo::init(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).at(1), noracc.count * 3 * 4);
            vkvbo::init(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).at(2), texacc.count * 2 * 4);

            vkebo::init(objs, mgltfobjs.rdgltfindexbufferdata.at(i).at(idx), idxacc.count * 2);

        }
    }

}



void staticmodel::uploadvboebo(vkobjs& objs) {
    for (size_t i{ 0 }; i < mmodel.meshes.size(); i++) {
        for (auto it = mmodel.meshes[i].primitives.begin(); it < mmodel.meshes[i].primitives.end(); it++) {
            const auto& idx = std::distance(mmodel.meshes[i].primitives.begin(), it);

            const fastgltf::Accessor& idxacc = mmodel.accessors[it->indicesAccessor.value()];

            const fastgltf::Accessor& posacc = mmodel.accessors[it->findAttribute("POSITION")->accessorIndex];
            const fastgltf::Accessor& noracc = mmodel.accessors[it->findAttribute("NORMAL")->accessorIndex];
            const fastgltf::Accessor& texacc = mmodel.accessors[it->findAttribute("TEXCOORD_0")->accessorIndex];

            const fastgltf::BufferView& idxbview = mmodel.bufferViews[idxacc.bufferViewIndex.value()];

            const fastgltf::BufferView& posbview = mmodel.bufferViews[posacc.bufferViewIndex.value()];
            const fastgltf::BufferView& norbview = mmodel.bufferViews[noracc.bufferViewIndex.value()];
            const fastgltf::BufferView& texbview = mmodel.bufferViews[texacc.bufferViewIndex.value()];

            const fastgltf::Buffer& b = mmodel.buffers[0];

            vkebo::upload(objs, mgltfobjs.rdgltfindexbufferdata.at(i).at(idx), b, idxbview, idxacc.count);


            vkvbo::upload(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).at(0), b, posbview, posacc);
            vkvbo::upload(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).at(1), b, norbview, noracc);
            vkvbo::upload(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(idx).at(2), b, texbview, texacc);


        }
    }

}





int staticmodel::gettricount(int i,int j) {
    const fastgltf::Primitive& prims = mmodel.meshes.at(i).primitives.at(j);
    const fastgltf::Accessor& acc = mmodel.accessors.at(prims.indicesAccessor.value());
    unsigned int c{ 0 };
    c = acc.count / 3;
    return c;
}









    //vkCmdBindDescriptorSets(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objs.rdgltfpipelinelayout, 0, 1, &mgltfobjs.rdgltfmodeltex.texdescriptorset, 0, nullptr);

    //VkDeviceSize ofx{ 0 };
    //for (int i{ 0 }; i < 3; ++i) {
    //    vkCmdBindVertexBuffers(objs.rdcommandbuffer, i, 1, &gltfobjs.rdgltfvertexbufferdata.at(i).rdvertexbuffer, &ofx);
    //}

    //vkCmdBindIndexBuffer(objs.rdcommandbuffer, gltfobjs.rdgltfindexbufferdata.rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);

    //vkCmdBindPipeline(objs.rdcommandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objs.rdgltfpipeline);

    //vkCmdDrawIndexed(objs.rdcommandbuffer, static_cast<uint32_t>(objs.rdgltftricount * 3), 1, 0, 0, 0);



void staticmodel::drawinstanced(vkobjs& objs,VkPipelineLayout& vkplayout, int instancecount, int stride) {
    VkDeviceSize offset = 0;
    std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.rdgltfvertexbufferdata.size());

    vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.texpls.texdescriptorset, 0, nullptr);

    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {

        pushes[i].reserve(mgltfobjs.rdgltfvertexbufferdata.at(i).size());
        pushes[i].resize(mgltfobjs.rdgltfvertexbufferdata.at(i).size());

        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            pushes[i][j].pkmodelstride = stride;
            pushes[i][j].texidx = static_cast<unsigned int>(mmodel.textures[mmodel.materials[mmodel.meshes.at(i).primitives.at(j).materialIndex.value()].pbrData.baseColorTexture->textureIndex].imageIndex.value());
            pushes[i][j].t = (float)glfwGetTime();

            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants), &pushes.at(i).at(j));


            for (int k{ 0 }; k < mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).size(); k++) {
                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).at(k).rdvertexbuffer, &offset);
            }
            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.rdgltfindexbufferdata.at(i).at(j).rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);
            //ubo::upload(objs, objs.rdperspviewmatrixubo, mmodel->textures[mmodel->materials[i].pbrMetallicRoughness.baseColorTexture.index].source);
            vkCmdDrawIndexed(objs.rdcommandbuffer[0], static_cast<uint32_t>(gettricount(i,j) * 3), instancecount, 0, 0, 0);
        }
    }

}

void staticmodel::cleanup(vkobjs& objs) {

    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {
            for (int k{ 0 }; k < mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).size(); k++) {
                vkvbo::cleanup(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).at(k));
            }
        }
    }
    for (int i{ 0 }; i < mgltfobjs.rdgltfindexbufferdata.size(); i++) {
        for (int j{ 0 }; j < mgltfobjs.rdgltfindexbufferdata.at(i).size(); j++) {
            vkebo::cleanup(objs, mgltfobjs.rdgltfindexbufferdata.at(i).at(j));
        }
    }
    vktexture::cleanuppls(objs, mgltfobjs.texpls);
    for (int i{ 0 }; i < mgltfobjs.tex.size(); i++) {
        vktexture::cleanup(objs, mgltfobjs.tex[i]);
    }

}


std::vector<vktexdata> staticmodel::gettexdata() {
    return mgltfobjs.tex;
}

vktexdatapls staticmodel::gettexdatapls(){
    return mgltfobjs.texpls;
}
