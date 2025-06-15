#pragma once

#include "vkobjs.hpp"
#include <vulkan/vulkan.h>

namespace framebuffer {
static inline bool create(vkobjs &rdata) {
	rdata.schainimgs = rdata.schain.get_images().value();
	rdata.schainimgviews = rdata.schain.get_image_views().value();

	rdata.fbuffers.reserve(rdata.schainimgviews.size());
	rdata.fbuffers.resize(rdata.schainimgviews.size());

	for (size_t i = 0; i < rdata.schainimgviews.size(); ++i) {
		VkImageView a[] = {rdata.schainimgviews[i], rdata.rddepthimageview};

		VkFramebufferCreateInfo fbinfo{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = rdata.rdrenderpass,
		.attachmentCount = 2,
		.pAttachments = a,
		.width = rdata.schain.extent.width,
		.height = rdata.schain.extent.height,
		.layers = 1
		};

		if (vkCreateFramebuffer(rdata.vkdevice.device, &fbinfo, nullptr, &rdata.fbuffers[i]) != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}
static inline void destroy(const VkDevice &dev,const std::span<VkFramebuffer>& fbs) {
	for (auto &fb : fbs) {
		vkDestroyFramebuffer(dev, fb, nullptr);
	}
}

};
