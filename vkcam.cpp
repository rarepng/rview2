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

    mright = glm::normalize(glm::cross(mforward, wup));
    mup = glm::normalize(glm::cross(mright, mforward));

    mvkobjs.rdcamwpos +=
        mvkobjs.rdcamforward * mvkobjs.rdtickdiff * mforward +
        mvkobjs.rdcamright * mvkobjs.rdtickdiff * mright +
        mvkobjs.rdcamup * mvkobjs.rdtickdiff * mup;




    return glm::lookAt(mvkobjs.rdcamwpos,mvkobjs.rdcamwpos+mforward,mup);
}
