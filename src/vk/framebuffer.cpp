#include "framebuffer.hpp"
#include <vector>

bool framebuffer::init(vkobjs &rdata) {
	rdata.schainimgs = rdata.schain.get_images().value();
	rdata.schainimgviews = rdata.schain.get_image_views().value();

	rdata.fbuffers.reserve(rdata.schainimgviews.size());
	rdata.fbuffers.resize(rdata.schainimgviews.size());

	for (unsigned int i = 0; i < rdata.schainimgviews.size(); ++i) {
		VkImageView a[] = {rdata.schainimgviews.at(i), rdata.rddepthimageview};

		VkFramebufferCreateInfo fbinfo{};
		fbinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbinfo.renderPass = rdata.rdrenderpass;
		fbinfo.attachmentCount = 2;
		fbinfo.pAttachments = a;
		fbinfo.width = rdata.schain.extent.width;
		fbinfo.height = rdata.schain.extent.height;
		fbinfo.layers = 1;

		if (vkCreateFramebuffer(rdata.vkdevice.device, &fbinfo, nullptr, &rdata.fbuffers[i]) != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}
void framebuffer::cleanup(vkobjs &rdata) {
	for (auto &fb : rdata.fbuffers) {
		vkDestroyFramebuffer(rdata.vkdevice.device, fb, nullptr);
	}
}
