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
#include "menubgmodel.hpp"

bool menubgmodel::loadmodel(vkobjs& objs){

    //mgltfobjs.rdgltfmodeltex.reserve(1);
    //mgltfobjs.rdgltfmodeltex.resize(1);

    //if (!vktexture::loadtexture(objs, mgltfobjs.rdgltfmodeltex, mmodel))return false;
    //if (!vktexture::loadtexlayoutpool(objs, mgltfobjs.rdgltfmodeltex, mmodel))return false;



    createvertexbuffers(objs);
    createindexbuffers(objs);

    return true;
}




void menubgmodel::createvertexbuffers(vkobjs& objs) {
    mgltfobjs.rdgltfvertexbufferdata.reserve(1);
    mgltfobjs.rdgltfvertexbufferdata.resize(1);
    mgltfobjs.rdgltfvertexbufferdata.at(0).reserve(1);
    mgltfobjs.rdgltfvertexbufferdata.at(0).resize(1);
    mgltfobjs.rdgltfvertexbufferdata.at(0).at(0).reserve(1);
    mgltfobjs.rdgltfvertexbufferdata.at(0).at(0).resize(1);
    vkvbo::init(objs, mgltfobjs.rdgltfvertexbufferdata.at(0).at(0).at(0), mmodel.size()*sizeof(glm::vec3));
}


void menubgmodel::createindexbuffers(vkobjs& objs) {
    mgltfobjs.rdgltfindexbufferdata.reserve(1);
    mgltfobjs.rdgltfindexbufferdata.resize(1);
    mgltfobjs.rdgltfindexbufferdata.at(0).reserve(1);
    mgltfobjs.rdgltfindexbufferdata.at(0).resize(1);
    vkebo::init(objs, mgltfobjs.rdgltfindexbufferdata.at(0).at(0), mindices.size() * sizeof(unsigned short));
}


void menubgmodel::uploadvertexbuffers(vkobjs& objs) {

    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); ++i) {
        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); ++j) {
            for (int k{ 0 }; k < mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).size(); ++k) {
                vkvbo::upload(objs, mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).at(k), mmodel);
            }
        }
    }

}

void menubgmodel::uploadindexbuffers(vkobjs& objs) {
    vkebo::upload(objs, mgltfobjs.rdgltfindexbufferdata.at(0).at(0), mindices);
}



void menubgmodel::draw(vkobjs& objs, VkPipelineLayout& vkplayout, double& time, double& time2,double& life) {
    VkDeviceSize offset = 0;
    //vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 0, nullptr, 0, nullptr);
    double pushes[] = { time,time2,life };
    for (int i{ 0 }; i < mgltfobjs.rdgltfvertexbufferdata.size(); i++) {

        for (int j{ 0 }; j < mgltfobjs.rdgltfvertexbufferdata.at(i).size(); j++) {

            vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 3 * sizeof(double), &pushes);


            for (int k{ 0 }; k < mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).size(); k++) {
                vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.rdgltfvertexbufferdata.at(i).at(j).at(k).rdvertexbuffer, &offset);
            }
            vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.rdgltfindexbufferdata.at(i).at(j).rdindexbuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(objs.rdcommandbuffer[0],6, 1, 0, 0, 0);
        }
    }

}



void menubgmodel::cleanup(vkobjs& objs) {

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
    for (int i{ 0 }; i < mgltfobjs.rdgltfmodeltex.size(); i++) {
        vktexture::cleanup(objs, mgltfobjs.rdgltfmodeltex[i]);
    }
    mmodel.clear();
    mindices.clear();

}


//std::vector<vktexdata> menubgmodel::gettexdata() {
//    return mgltfobjs.rdgltfmodeltex;
//}
