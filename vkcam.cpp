#define _USE_MATH_DEFINES
#include <cmath>
#include "vkcam.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>


glm::mat4 vkcam::getview(vkobjs& mvkobjs)
{
    mforward = glm::normalize(glm::vec3{
        glm::sin(glm::radians(mvkobjs.rdazimuth)) * glm::cos(glm::radians(mvkobjs.rdelevation)),
        glm::sin(glm::radians(mvkobjs.rdelevation)),
        -glm::cos(glm::radians(mvkobjs.rdazimuth)) * glm::cos(glm::radians(mvkobjs.rdelevation))
    });
    //mforward = glm::vec3{ 0.0f,0.0f,-1.0f };

    mright = glm::normalize(glm::cross(mforward, wup));
    //mright = glm::vec3{ 1.0f,0.0f,0.0f };

    mup = glm::normalize(glm::cross(mright, mforward));
    //mup = glm::vec3{ 0.0f,1.0f,0.0f };

    mvkobjs.rdcamwpos +=
        mvkobjs.rdcamforward    * static_cast<float>(mvkobjs.tickdiff) * mforward +
        mvkobjs.rdcamright      * static_cast<float>(mvkobjs.tickdiff) * mright +
        mvkobjs.rdcamup         * static_cast<float>(mvkobjs.tickdiff) * mup;




    return glm::lookAt(mvkobjs.rdcamwpos,mvkobjs.rdcamwpos+mforward,mup);
}
