#include "framebuffer.hpp"

bool framebuffer::init(vkobjs& rdata) {
	rdata.rdswapchainimages = rdata.rdvkbswapchain.get_images().value();
	rdata.rdswapchainimageviews = rdata.rdvkbswapchain.get_image_views().value();

	rdata.rdframebuffers.reserve(rdata.rdswapchainimageviews.size());
	rdata.rdframebuffers.resize(rdata.rdswapchainimageviews.size());

	for (unsigned int i = 0; i < rdata.rdswapchainimageviews.size(); ++i) {
		VkImageView a[] = {
			rdata.rdswapchainimageviews.at(i),
			rdata.rddepthimageview
		};

		VkFramebufferCreateInfo fbinfo{};
		fbinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbinfo.renderPass = rdata.rdrenderpass;
		fbinfo.attachmentCount = 2;
		fbinfo.pAttachments = a;
		fbinfo.width = rdata.rdvkbswapchain.extent.width;
		fbinfo.height = rdata.rdvkbswapchain.extent.height;
		fbinfo.layers = 1;

		if (vkCreateFramebuffer(rdata.rdvkbdevice.device, &fbinfo, nullptr, &rdata.rdframebuffers[i]) != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}
bool framebuffer::initref(vkobjs& rdata) {

	rdata.rdframebufferrefs.reserve(rdata.rdswapchainimageviews.size());
	rdata.rdframebufferrefs.resize(rdata.rdswapchainimageviews.size());

	for (unsigned int i = 0; i < rdata.rdswapchainimageviews.size(); ++i) {
		VkImageView a[] = {
			rdata.rdswapchainimageviews.at(i),
			rdata.rddepthimageviewref
		};

		VkFramebufferCreateInfo fbinfo{};
		fbinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbinfo.renderPass = rdata.rdrenderpass2;
		fbinfo.attachmentCount = 2;
		fbinfo.pAttachments = a;
		fbinfo.width = rdata.rdvkbswapchain.extent.width;
		fbinfo.height = rdata.rdvkbswapchain.extent.height;
		fbinfo.layers = 1;

		if (vkCreateFramebuffer(rdata.rdvkbdevice.device, &fbinfo, nullptr, &rdata.rdframebufferrefs[i]) != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}
void framebuffer::cleanup(vkobjs& rdata) {
	for (auto& fb : rdata.rdframebuffers) {
		vkDestroyFramebuffer(rdata.rdvkbdevice.device, fb, nullptr);
	}
}
