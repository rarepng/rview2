#include "renderpass.hpp"
#include <VkBootstrap.h>

bool renderpass::init(rvk &rdata) {
	VkAttachmentDescription colora{};
	colora.format = rdata.schain.image_format;
	colora.samples = VK_SAMPLE_COUNT_1_BIT;
	colora.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colora.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colora.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colora.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colora.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colora.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference coloraref{};
	coloraref.attachment = 0;
	coloraref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription deptha{};
	deptha.flags = 0;
	deptha.format = rdata.rddepthformat;
	deptha.samples = VK_SAMPLE_COUNT_1_BIT;
	deptha.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	deptha.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	deptha.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	deptha.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	deptha.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	deptha.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference deptharef{};
	deptharef.attachment = 1;
	deptharef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassdesc{};
	subpassdesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassdesc.colorAttachmentCount = 1;
	subpassdesc.pColorAttachments = &coloraref;
	subpassdesc.pDepthStencilAttachment = &deptharef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency deps[] = {dependency};
	VkAttachmentDescription descs[] = {colora, deptha};

	VkRenderPassCreateInfo renderpassinfo{};
	renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpassinfo.attachmentCount = 2;
	renderpassinfo.pAttachments = descs;
	renderpassinfo.subpassCount = 1;
	renderpassinfo.pSubpasses = &subpassdesc;
	renderpassinfo.dependencyCount = 1;
	renderpassinfo.pDependencies = deps;

	// if (vkCreateRenderPass(rdata.vkdevice.device, &renderpassinfo, nullptr, &rdata.rdrenderpass) != VK_SUCCESS) {
	// 	return false;
	// }

	return true;
}
void renderpass::cleanup(rvk &rdata) {
	// vkDestroyRenderPass(rdata.vkdevice.device, rdata.rdrenderpass, nullptr);
}
