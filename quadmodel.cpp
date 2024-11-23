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
#include "quadmodel.hpp"

bool quadmodel::loadmodel(vkobjs& objs){

    createvertexbuffers(objs);
    createindexbuffers(objs);

    return true;
}




void quadmodel::createvertexbuffers(vkobjs& objs) {
    mgltfobjs.vbodata.reserve(1);
    mgltfobjs.vbodata.resize(1);
    mgltfobjs.vbodata.at(0).reserve(1);
    mgltfobjs.vbodata.at(0).resize(1);
    mgltfobjs.vbodata.at(0).at(0).reserve(1);
    mgltfobjs.vbodata.at(0).at(0).resize(1);
    vkvbo::init(objs, mgltfobjs.vbodata.at(0).at(0).at(0), mmodel.size()*sizeof(glm::vec3));
}


void quadmodel::createindexbuffers(vkobjs& objs) {
    mgltfobjs.ebodata.reserve(1);
    mgltfobjs.ebodata.resize(1);
    mgltfobjs.ebodata.at(0).reserve(1);
    mgltfobjs.ebodata.at(0).resize(1);
    vkebo::init(objs, mgltfobjs.ebodata.at(0).at(0), mindices.size() * sizeof(unsigned short));
}


void quadmodel::uploadvboebo(vkobjs& objs, VkCommandBuffer& cbuffer) {

    vkebo::upload(objs, cbuffer, mgltfobjs.ebodata.at(0).at(0), mindices);

    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); ++i) {
        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); ++j) {
            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); ++k) {
                vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(j).at(k), mmodel);
            }
        }
    }

}



void quadmodel::draw(vkobjs& objs, VkPipelineLayout& vkplayout, double& time, double& time2,double& life) {
    VkDeviceSize offset = 0;
    //vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 0, nullptr, 0, nullptr);
    double pushes[] = { time,time2,life };
    for (int i{ 0 }; i < mgltfobjs.vbodata.size(); i++) {

        for (int j{ 0 }; j < mgltfobjs.vbodata.at(i).size(); j++) {

            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 3 * sizeof(double), &pushes);


            for (int k{ 0 }; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer, &offset);
            }
            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.ebodata.at(i).at(j).bhandle, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(objs.rdcommandbuffer[0],6, 1, 0, 0, 0);
        }
    }

}



void quadmodel::cleanup(vkobjs& objs) {

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
    vktexture::cleanuppls(objs, mgltfobjs.texpls);
    for (int i{ 0 }; i < mgltfobjs.tex.size(); i++) {
        vktexture::cleanup(objs, mgltfobjs.tex[i]);
    }
    mmodel.clear();
    mindices.clear();

}
