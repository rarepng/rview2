#include "vksyncobjects.hpp"
#include <algorithm>
#include <tuple>

bool vksyncobjects::init(rvkbucket &rdata) {
    VkFenceCreateInfo fenceinfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkSemaphoreCreateInfo semaphoreinfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkDevice dev = rdata.core.vkdevice.device;

    return std::ranges::all_of(rdata.framez, [&](framedata& frame) {
        bool sems_ok = std::ranges::all_of(frame.semaphorez, [&](VkSemaphore& sem) {
            if (vkCreateSemaphore(dev, &semaphoreinfo, nullptr, &sem) == VK_SUCCESS) {
                rdata.cleanupQ.push_function([dev, sem]() {
                    vkDestroySemaphore(dev, sem, nullptr);
                });
                return true;
            }
            return false;
        });
        if (!sems_ok) return false;
        if (vkCreateFence(dev, &fenceinfo, nullptr, &frame.fencez) == VK_SUCCESS) {
            VkFence f = frame.fencez;
            rdata.cleanupQ.push_function([dev, f]() {
                vkDestroyFence(dev, f, nullptr);
            });
            return true;
        }

        return false;
    });
}
