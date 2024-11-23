#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <cstring>
#include "commandbuffer.hpp"
#include "vktexture.hpp"
#include <vk/VkBootstrap.h>
#include <future>
#include <thread>
#include <mutex>
#include "logger.hpp"

bool vktexture::loadtexturefile(vkobjs& rdata, vktexdata& texdata, vktexdatapls& texdatapls, std::string filename) {

	int w;
	int h;
	int c;

	unsigned char* data = stbi_load(filename.c_str(), &w, &h, &c, STBI_rgb_alpha);
	if (!data) {
		stbi_image_free(data);
		return false;
	}


	VkDeviceSize imgsize = w * h * 4;

	VkImageCreateInfo imginfo{};
	imginfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imginfo.imageType = VK_IMAGE_TYPE_2D;
	imginfo.extent.width = static_cast<uint32_t>(w);
	imginfo.extent.height = static_cast<uint32_t>(h);
	imginfo.extent.depth = 1;
	imginfo.mipLevels = 1;
	imginfo.arrayLayers = 1;
	imginfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imginfo.tiling = VK_IMAGE_TILING_LINEAR;
	imginfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imginfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imginfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imginfo.samples = VK_SAMPLE_COUNT_1_BIT;





	VmaAllocationCreateInfo iainfo{};
	iainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	if (vmaCreateImage(rdata.rdallocator, &imginfo, &iainfo, &texdata.teximg, &texdata.teximgalloc, nullptr) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vmaCreateImage");
		return false;
	}





	VkBufferCreateInfo stagingbufferinfo{};
	stagingbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingbufferinfo.size = imgsize;
	stagingbufferinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VkBuffer stagingbuffer;
	VmaAllocation stagingbufferalloc;

	VmaAllocationCreateInfo stagingallocinfo{};
	stagingallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;


	if (vmaCreateBuffer(rdata.rdallocator, &stagingbufferinfo, &stagingallocinfo, &stagingbuffer, &stagingbufferalloc, nullptr) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vmaCreateBuffer");
		return false;
	}


	void* tmp;
	vmaMapMemory(rdata.rdallocator, stagingbufferalloc, &tmp);
	std::memcpy(tmp, data, (imgsize));
	vmaUnmapMemory(rdata.rdallocator, stagingbufferalloc);

	stbi_image_free(data);



	VkImageSubresourceRange stagingbufferrange{};
	stagingbufferrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	stagingbufferrange.baseMipLevel = 0;
	stagingbufferrange.levelCount = 1;
	stagingbufferrange.baseArrayLayer = 0;
	stagingbufferrange.layerCount = 1;

	VkImageMemoryBarrier stagingbuffertransferbarrier{};
	stagingbuffertransferbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	stagingbuffertransferbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	stagingbuffertransferbarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	stagingbuffertransferbarrier.image = texdata.teximg;
	stagingbuffertransferbarrier.subresourceRange = stagingbufferrange;
	stagingbuffertransferbarrier.srcAccessMask = 0;
	stagingbuffertransferbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;



	VkExtent3D texextent{};
	texextent.width = static_cast<uint32_t>(w);
	texextent.height = static_cast<uint32_t>(h);
	texextent.depth = 1;

	VkBufferImageCopy stagingbuffercopy{};
	stagingbuffercopy.bufferOffset = 0;
	stagingbuffercopy.bufferRowLength = 0;
	stagingbuffercopy.bufferImageHeight = 0;
	stagingbuffercopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	stagingbuffercopy.imageSubresource.mipLevel = 0;
	stagingbuffercopy.imageSubresource.baseArrayLayer = 0;
	stagingbuffercopy.imageSubresource.layerCount = 1;
	stagingbuffercopy.imageExtent = texextent;





	VkImageMemoryBarrier stagingbuffershaderbarrier{};
	stagingbuffershaderbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	stagingbuffershaderbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	stagingbuffershaderbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	stagingbuffershaderbarrier.image = texdata.teximg;
	stagingbuffershaderbarrier.subresourceRange = stagingbufferrange;
	stagingbuffershaderbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	stagingbuffershaderbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;


	VkCommandBuffer stagingcommandbuffer;
	if (!commandbuffer::init(rdata,rdata.rdcommandpool[2], stagingcommandbuffer)) {
		logger::log(0,"crashed in texture at commandbuffer::init");
		return false;
	}

	if (vkResetCommandBuffer(stagingcommandbuffer, 0) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkResetCommandBuffer");
		return false;
	}



	VkCommandBufferBeginInfo cmdbegininfo{};
	cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(stagingcommandbuffer, &cmdbegininfo) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkBeginCommandBuffer");
		return false;
	}

	vkCmdPipelineBarrier(stagingcommandbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stagingbuffertransferbarrier);
	vkCmdCopyBufferToImage(stagingcommandbuffer, stagingbuffer, texdata.teximg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &stagingbuffercopy);
	vkCmdPipelineBarrier(stagingcommandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stagingbuffershaderbarrier);


	if (vkEndCommandBuffer(stagingcommandbuffer) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkEndCommandBuffer");
		return false;
	}

	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.pWaitDstStageMask = nullptr;
	submitinfo.waitSemaphoreCount = 0;
	submitinfo.pWaitSemaphores = nullptr;
	submitinfo.signalSemaphoreCount = 0;
	submitinfo.pSignalSemaphores = nullptr;
	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &stagingcommandbuffer;

	VkFence stagingbufferfence;

	VkFenceCreateInfo fenceinfo{};
	fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;


	if (vkCreateFence(rdata.rdvkbdevice.device, &fenceinfo, nullptr, &stagingbufferfence) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkCreateFence");
		return false;
	}
	if (vkResetFences(rdata.rdvkbdevice.device, 1, &stagingbufferfence) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkResetFences");
		return false;
	}
	rdata.mtx2->lock();
	if (vkQueueSubmit(rdata.rdgraphicsqueue, 1, &submitinfo, stagingbufferfence) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkQueueSubmit");
		return false;
	}
	rdata.mtx2->unlock();
	if (vkWaitForFences(rdata.rdvkbdevice.device, 1, &stagingbufferfence, VK_TRUE, INT64_MAX) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkWaitForFences");
		return false;
	}




	vkDestroyFence(rdata.rdvkbdevice.device, stagingbufferfence, nullptr);
	commandbuffer::cleanup(rdata, rdata.rdcommandpool[2], stagingcommandbuffer);
	vmaDestroyBuffer(rdata.rdallocator, stagingbuffer, stagingbufferalloc);

	VkImageViewCreateInfo texviewinfo{};
	texviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	texviewinfo.image = texdata.teximg;
	texviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	texviewinfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	texviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	texviewinfo.subresourceRange.baseMipLevel = 0;
	texviewinfo.subresourceRange.levelCount = 1;
	texviewinfo.subresourceRange.baseArrayLayer = 0;
	texviewinfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(rdata.rdvkbdevice.device, &texviewinfo, nullptr, &texdata.teximgview) != VK_SUCCESS)
	{
		logger::log(0,"crashed in texture at vkCreateImageView");
		return false;
	}

	VkSamplerCreateInfo texsamplerinfo{};
	texsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	texsamplerinfo.magFilter = VK_FILTER_LINEAR;
	texsamplerinfo.minFilter = VK_FILTER_LINEAR;
	texsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	texsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	texsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	texsamplerinfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	texsamplerinfo.unnormalizedCoordinates = VK_FALSE;
	texsamplerinfo.compareEnable = VK_FALSE;
	texsamplerinfo.compareOp = VK_COMPARE_OP_ALWAYS;
	texsamplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	texsamplerinfo.mipLodBias = 0.0f;
	texsamplerinfo.minLod = 0.0f;
	texsamplerinfo.maxLod = 0.0f;
	texsamplerinfo.anisotropyEnable = VK_FALSE;
	texsamplerinfo.maxAnisotropy = 1.0f;


	if (vkCreateSampler(rdata.rdvkbdevice.device, &texsamplerinfo, nullptr, &texdata.texsampler) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkCreateSampler");
		return false;
	}

	VkDescriptorSetLayoutBinding texturebind{};
	texturebind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturebind.binding = 0;
	texturebind.descriptorCount = 1;
	texturebind.pImmutableSamplers = nullptr;
	texturebind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo texcreateinfo{};
	texcreateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	texcreateinfo.bindingCount = 1;
	texcreateinfo.pBindings = &texturebind;

	if (vkCreateDescriptorSetLayout(rdata.rdvkbdevice.device, &texcreateinfo, nullptr, &texdatapls.texdescriptorlayout) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkCreateDescriptorSetLayout");
		return false;
	}


	VkDescriptorPoolSize poolsize{};
	poolsize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolsize.descriptorCount = imgsize;

	VkDescriptorPoolCreateInfo descriptorpool{};
	descriptorpool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorpool.poolSizeCount = 1;
	descriptorpool.pPoolSizes = &poolsize;
	descriptorpool.maxSets = 16;

	if (vkCreateDescriptorPool(rdata.rdvkbdevice.device, &descriptorpool, nullptr, &texdatapls.texdescriptorpool) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkCreateDescriptorPool");
		return false;
	}


	VkDescriptorSetAllocateInfo descallocinfo{};
	descallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descallocinfo.descriptorPool = texdatapls.texdescriptorpool;
	descallocinfo.descriptorSetCount = 1;
	descallocinfo.pSetLayouts = &texdatapls.texdescriptorlayout;


	if (vkAllocateDescriptorSets(rdata.rdvkbdevice.device, &descallocinfo, &texdatapls.texdescriptorset) != VK_SUCCESS) {
		logger::log(0,"crashed in texture at vkAllocateDescriptorSets");
		return false;
	}


	VkDescriptorImageInfo descriptorimginfo{};
	descriptorimginfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorimginfo.imageView = texdata.teximgview;
	descriptorimginfo.sampler = texdata.texsampler;

	VkWriteDescriptorSet writedescset{};
	writedescset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writedescset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writedescset.dstSet = texdatapls.texdescriptorset;
	writedescset.dstBinding = 0;
	writedescset.descriptorCount = 1;
	writedescset.pImageInfo = &descriptorimginfo;

	vkUpdateDescriptorSets(rdata.rdvkbdevice.device, 1, &writedescset, 0, nullptr);

	return true;

}


//
//bool vktexture::loadtexture(vkobjs& rdata, std::vector<vktexdata>& texdata, std::shared_ptr<tinygltf::Model> mmodel){
//	
//	texdata.reserve(mmodel->images.size());
//	texdata.resize(mmodel->images.size());
//	for (int i{ 0 }; i < mmodel->images.size(); i++) {
//
//
//	int w=mmodel->images[i].width;
//	int h = mmodel->images[i].height;
//	//int c;
//
//
//
//
//	unsigned char* data = mmodel->images[i].image.data();
//	if (!data) {
//		stbi_image_free(data);
//		return false;
//	}
//
//	//uint32_t mlvls = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;
//
//	VkDeviceSize imgsize = w * h * 4;
//	VkImageCreateInfo imginfo{};
//	imginfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//	imginfo.imageType = VK_IMAGE_TYPE_2D;
//	imginfo.extent.width = static_cast<uint32_t>(w);
//	imginfo.extent.height = static_cast<uint32_t>(h);
//	imginfo.extent.depth = 1;
//	imginfo.mipLevels = 1;
//	imginfo.arrayLayers = 1;
//	imginfo.format = VK_FORMAT_R8G8B8A8_SRGB;
//	imginfo.tiling = VK_IMAGE_TILING_LINEAR;
//	imginfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//	imginfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
//	imginfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//	imginfo.samples = VK_SAMPLE_COUNT_1_BIT;
//
//
//
//
//
//	VmaAllocationCreateInfo iainfo{};
//	iainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//	if (vmaCreateImage(rdata.rdallocator, &imginfo, &iainfo, &texdata[i].teximg, &texdata[i].teximgalloc, nullptr) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vmaCreateImage");
//		return false;
//	}
//
//
//
//
//
//	VkBufferCreateInfo stagingbufferinfo{};
//	stagingbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
//	stagingbufferinfo.size = imgsize;
//	stagingbufferinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//
//	VkBuffer stagingbuffer;
//	VmaAllocation stagingbufferalloc;
//
//	VmaAllocationCreateInfo stagingallocinfo{};
//	stagingallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//
//
//	if (vmaCreateBuffer(rdata.rdallocator, &stagingbufferinfo, &stagingallocinfo, &stagingbuffer, &stagingbufferalloc, nullptr) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vmaCreateBuffer");
//		return false;
//	}
//
//
//	void* tmp;
//	vmaMapMemory(rdata.rdallocator, stagingbufferalloc, &tmp);
//	std::memcpy(tmp, data, (imgsize));////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	vmaUnmapMemory(rdata.rdallocator, stagingbufferalloc);
//
//	//stbi_image_free(data);
//
//
//
//
//	VkImageSubresourceRange stagingbufferrange{};
//	stagingbufferrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	stagingbufferrange.baseMipLevel = 0;
//	stagingbufferrange.levelCount = 1;
//	stagingbufferrange.baseArrayLayer = 0;
//	stagingbufferrange.layerCount = 1;
//
//	VkImageMemoryBarrier stagingbuffertransferbarrier{};
//	stagingbuffertransferbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	stagingbuffertransferbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//	stagingbuffertransferbarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//	stagingbuffertransferbarrier.image = texdata[i].teximg;
//	stagingbuffertransferbarrier.subresourceRange = stagingbufferrange;
//	stagingbuffertransferbarrier.srcAccessMask = 0;
//	stagingbuffertransferbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//
//
//
//	VkExtent3D texextent{};
//	texextent.width = static_cast<uint32_t>(w);
//	texextent.height = static_cast<uint32_t>(h);
//	texextent.depth = 1;
//
//	VkBufferImageCopy stagingbuffercopy{};
//	stagingbuffercopy.bufferOffset = 0;
//	stagingbuffercopy.bufferRowLength = 0;
//	stagingbuffercopy.bufferImageHeight = 0;
//	stagingbuffercopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	stagingbuffercopy.imageSubresource.mipLevel = 0;
//	stagingbuffercopy.imageSubresource.baseArrayLayer = 0;
//	stagingbuffercopy.imageSubresource.layerCount = 1;
//	stagingbuffercopy.imageExtent = texextent;
//
//
//
//
//
//	VkImageMemoryBarrier stagingbuffershaderbarrier{};
//	stagingbuffershaderbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//	stagingbuffershaderbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//	stagingbuffershaderbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//	stagingbuffershaderbarrier.image = texdata[i].teximg;
//	stagingbuffershaderbarrier.subresourceRange = stagingbufferrange;
//	stagingbuffershaderbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//	stagingbuffershaderbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//
//
//	VkCommandBuffer stagingcommandbuffer;
//	if (!commandbuffer::init(rdata, rdata.rdcommandpool[0], stagingcommandbuffer)) {
//		logger::log(0,"crashed in texture at commandbuffer::init");
//		return false;
//	}
//
//	if (vkResetCommandBuffer(stagingcommandbuffer, 0) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkResetCommandBuffer");
//		return false;
//	}
//
//
//
//	VkCommandBufferBeginInfo cmdbegininfo{};
//	cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//	cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//
//	if (vkBeginCommandBuffer(stagingcommandbuffer, &cmdbegininfo) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkBeginCommandBuffer");
//		return false;
//	}
//
//	vkCmdPipelineBarrier(stagingcommandbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stagingbuffertransferbarrier);
//	vkCmdCopyBufferToImage(stagingcommandbuffer, stagingbuffer, texdata[i].teximg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &stagingbuffercopy);
//	vkCmdPipelineBarrier(stagingcommandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stagingbuffershaderbarrier);
//
//
//	if (vkEndCommandBuffer(stagingcommandbuffer) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkEndCommandBuffer");
//		return false;
//	}
//
//	VkSubmitInfo submitinfo{};
//	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//	submitinfo.pWaitDstStageMask = nullptr;
//	submitinfo.waitSemaphoreCount = 0;
//	submitinfo.pWaitSemaphores = nullptr;
//	submitinfo.signalSemaphoreCount = 0;
//	submitinfo.pSignalSemaphores = nullptr;
//	submitinfo.commandBufferCount = 1;
//	submitinfo.pCommandBuffers = &stagingcommandbuffer;
//
//	VkFence stagingbufferfence;
//
//	VkFenceCreateInfo fenceinfo{};
//	fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
//	fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
//
//
//	if (vkCreateFence(rdata.rdvkbdevice.device, &fenceinfo, nullptr, &stagingbufferfence) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkCreateFence");
//		return false;
//	}
//	if (vkResetFences(rdata.rdvkbdevice.device, 1, &stagingbufferfence) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkResetFences");
//		return false;
//	}
//	//if (vkWaitForFences(rdata.rdvkbdevice.device, 1, &rdata.rdrenderfence, VK_TRUE, INT64_MAX) != VK_SUCCESS) {
//	//	return false;
//	//}
//
//	rdata.mtx2->lock();
//	if (vkQueueSubmit(rdata.rdgraphicsqueue, 1, &submitinfo, stagingbufferfence) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkQueueSubmit");
//		return false;
//	}
//
//	rdata.mtx2->unlock();
//
//	if (vkWaitForFences(rdata.rdvkbdevice.device, 1, &stagingbufferfence, VK_TRUE, INT64_MAX) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkWaitForFences");
//		return false;
//	}
//
//
//
//
//	vkDestroyFence(rdata.rdvkbdevice.device, stagingbufferfence, nullptr);
//	commandbuffer::cleanup(rdata, rdata.rdcommandpool[0], stagingcommandbuffer);
//	vmaDestroyBuffer(rdata.rdallocator, stagingbuffer, stagingbufferalloc);
//
//	VkImageViewCreateInfo texviewinfo{};
//	texviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//	texviewinfo.image = texdata[i].teximg;
//	texviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//	texviewinfo.format = VK_FORMAT_R8G8B8A8_SRGB;
//	texviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//	texviewinfo.subresourceRange.baseMipLevel = 0;
//	texviewinfo.subresourceRange.levelCount = 1;
//	texviewinfo.subresourceRange.baseArrayLayer = 0;
//	texviewinfo.subresourceRange.layerCount = 1;
//
//	if (vkCreateImageView(rdata.rdvkbdevice.device, &texviewinfo, nullptr, &texdata[i].teximgview) != VK_SUCCESS)
//	{
//		logger::log(0,"crashed in texture at vkCreateImageView");
//		return false;
//	}
//
//	VkSamplerCreateInfo texsamplerinfo{};
//	texsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
//	texsamplerinfo.magFilter = VK_FILTER_LINEAR;
//	texsamplerinfo.minFilter = VK_FILTER_LINEAR;
//	texsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//	texsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//	texsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//	texsamplerinfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
//	texsamplerinfo.unnormalizedCoordinates = VK_FALSE;
//	texsamplerinfo.compareEnable = VK_FALSE;
//	texsamplerinfo.compareOp = VK_COMPARE_OP_ALWAYS;
//	texsamplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
//	texsamplerinfo.mipLodBias = 0.0f;
//	texsamplerinfo.minLod = 0.0f;
//	texsamplerinfo.maxLod = 0.0f;
//	texsamplerinfo.anisotropyEnable = VK_FALSE;
//	texsamplerinfo.maxAnisotropy = 1.0f;
//
//
//	if (vkCreateSampler(rdata.rdvkbdevice.device, &texsamplerinfo, nullptr, &texdata[i].texsampler) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkCreateSampler");
//		return false;
//	}
//
//	}
//
//	return true;
//}
//bool vktexture::loadtexlayoutpool(vkobjs& rdata, std::vector<vktexdata>& texdata, vktexdatapls& texdatapls, std::shared_ptr<tinygltf::Model> mmodel) {
//
//	VkDescriptorSetLayoutBinding texturebind;
//	std::vector<VkDescriptorImageInfo> descriptorimginfo;
//	descriptorimginfo.reserve(mmodel->images.size());
//	descriptorimginfo.resize(mmodel->images.size());
//
//	texturebind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//	texturebind.binding = 0;
//	texturebind.descriptorCount = mmodel->images.size();
//	texturebind.pImmutableSamplers = nullptr;
//	texturebind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
//
//	for (int i{ 0 }; i < mmodel->images.size(); i++) {
//
//		descriptorimginfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//		descriptorimginfo[i].imageView = texdata[i].teximgview;
//		descriptorimginfo[i].sampler = texdata[i].texsampler;
//	}
//
//
//
//	VkDescriptorSetLayoutCreateInfo texcreateinfo{};
//	texcreateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
//	texcreateinfo.bindingCount = 1;//mmodel->images.size();
//	texcreateinfo.pBindings = &texturebind;
//
//	if (vkCreateDescriptorSetLayout(rdata.rdvkbdevice.device, &texcreateinfo, nullptr, &texdatapls.texdescriptorlayout) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkCreateDescriptorSetLayout");
//		return false;
//	}
//
//
//	VkDescriptorPoolSize poolsize{};
//	poolsize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//	poolsize.descriptorCount = mmodel->images.size() * 1024 * 1024 * 4;
//
//	VkDescriptorPoolCreateInfo descriptorpool{};
//	descriptorpool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
//	descriptorpool.poolSizeCount = 1;
//	descriptorpool.pPoolSizes = &poolsize;
//	descriptorpool.maxSets = 16;
//
//	if (vkCreateDescriptorPool(rdata.rdvkbdevice.device, &descriptorpool, nullptr, &texdatapls.texdescriptorpool) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkCreateDescriptorPool");
//		return false;
//	}
//
//
//	VkDescriptorSetAllocateInfo descallocinfo{};
//	descallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//	descallocinfo.descriptorPool = texdatapls.texdescriptorpool;
//	descallocinfo.descriptorSetCount = 1;
//	descallocinfo.pSetLayouts = &texdatapls.texdescriptorlayout;
//
//
//	if (vkAllocateDescriptorSets(rdata.rdvkbdevice.device, &descallocinfo, &texdatapls.texdescriptorset) != VK_SUCCESS) {
//		logger::log(0,"crashed in texture at vkAllocateDescriptorSets");
//		return false;
//	}
//
//	VkWriteDescriptorSet writedescset{};
//	writedescset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//	writedescset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//	writedescset.dstSet = texdatapls.texdescriptorset;
//	writedescset.dstArrayElement = 0;
//	writedescset.dstBinding = 0;
//	//writedescset.pBufferInfo = 0;
//	writedescset.descriptorCount = mmodel->images.size();
//	writedescset.pImageInfo = descriptorimginfo.data();
//	vkUpdateDescriptorSets(rdata.rdvkbdevice.device, 1, &writedescset, 0, nullptr);
//
//	return true;
//}
//


bool vktexture::loadtexture(vkobjs& rdata, std::vector<vktexdata>& texdata, fastgltf::Asset& mmodel) {

	texdata.reserve(mmodel.images.size());
	texdata.resize(mmodel.images.size());
	for (int i{ 0 }; i < mmodel.images.size(); i++) {

		int w, h, c;

		unsigned char* data=nullptr;


		std::visit(fastgltf::visitor{
			[](auto& arg) {},
			[&](fastgltf::sources::BufferView& view) {
			auto& bufferView = mmodel.bufferViews[view.bufferViewIndex];
			auto& buffer = mmodel.buffers[bufferView.bufferIndex];
			std::visit(fastgltf::visitor {
				[](auto& arg) {},
				[&](fastgltf::sources::Array& vector) {
					data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset), static_cast<int>(bufferView.byteLength), &w, &h, &c, 4);

				} },buffer.data);
			} }, mmodel.images[i].data);

















		//if (!data) {
		//	stbi_image_free(data);
		//	return false;
		//}

		//uint32_t mlvls = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;

		VkDeviceSize imgsize = w * h * 4;
		VkImageCreateInfo imginfo{};
		imginfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imginfo.imageType = VK_IMAGE_TYPE_2D;
		imginfo.extent.width = static_cast<uint32_t>(w);
		imginfo.extent.height = static_cast<uint32_t>(h);
		imginfo.extent.depth = 1;
		imginfo.mipLevels = 1;
		imginfo.arrayLayers = 1;
		imginfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imginfo.tiling = VK_IMAGE_TILING_LINEAR;
		imginfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imginfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imginfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imginfo.samples = VK_SAMPLE_COUNT_1_BIT;





		VmaAllocationCreateInfo iainfo{};
		iainfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		if (vmaCreateImage(rdata.rdallocator, &imginfo, &iainfo, &texdata[i].teximg, &texdata[i].teximgalloc, nullptr) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vmaCreateImage");
			return false;
		}





		VkBufferCreateInfo stagingbufferinfo{};
		stagingbufferinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingbufferinfo.size = imgsize;
		stagingbufferinfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VkBuffer stagingbuffer;
		VmaAllocation stagingbufferalloc;

		VmaAllocationCreateInfo stagingallocinfo{};
		stagingallocinfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;


		if (vmaCreateBuffer(rdata.rdallocator, &stagingbufferinfo, &stagingallocinfo, &stagingbuffer, &stagingbufferalloc, nullptr) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vmaCreateBuffer");
			return false;
		}


		void* tmp;
		vmaMapMemory(rdata.rdallocator, stagingbufferalloc, &tmp);
		std::memcpy(tmp, data, (imgsize));////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		vmaUnmapMemory(rdata.rdallocator, stagingbufferalloc);

		stbi_image_free(data);




		VkImageSubresourceRange stagingbufferrange{};
		stagingbufferrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		stagingbufferrange.baseMipLevel = 0;
		stagingbufferrange.levelCount = 1;
		stagingbufferrange.baseArrayLayer = 0;
		stagingbufferrange.layerCount = 1;

		VkImageMemoryBarrier stagingbuffertransferbarrier{};
		stagingbuffertransferbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		stagingbuffertransferbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		stagingbuffertransferbarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		stagingbuffertransferbarrier.image = texdata[i].teximg;
		stagingbuffertransferbarrier.subresourceRange = stagingbufferrange;
		stagingbuffertransferbarrier.srcAccessMask = 0;
		stagingbuffertransferbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;



		VkExtent3D texextent{};
		texextent.width = static_cast<uint32_t>(w);
		texextent.height = static_cast<uint32_t>(h);
		texextent.depth = 1;

		VkBufferImageCopy stagingbuffercopy{};
		stagingbuffercopy.bufferOffset = 0;
		stagingbuffercopy.bufferRowLength = 0;
		stagingbuffercopy.bufferImageHeight = 0;
		stagingbuffercopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		stagingbuffercopy.imageSubresource.mipLevel = 0;
		stagingbuffercopy.imageSubresource.baseArrayLayer = 0;
		stagingbuffercopy.imageSubresource.layerCount = 1;
		stagingbuffercopy.imageExtent = texextent;





		VkImageMemoryBarrier stagingbuffershaderbarrier{};
		stagingbuffershaderbarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		stagingbuffershaderbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		stagingbuffershaderbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		stagingbuffershaderbarrier.image = texdata[i].teximg;
		stagingbuffershaderbarrier.subresourceRange = stagingbufferrange;
		stagingbuffershaderbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		stagingbuffershaderbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;


		VkCommandBuffer stagingcommandbuffer;
		if (!commandbuffer::init(rdata, rdata.rdcommandpool[2], stagingcommandbuffer)) {
			logger::log(0, "crashed in texture at commandbuffer::init");
			return false;
		}

		if (vkResetCommandBuffer(stagingcommandbuffer, 0) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkResetCommandBuffer");
			return false;
		}



		VkCommandBufferBeginInfo cmdbegininfo{};
		cmdbegininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdbegininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(stagingcommandbuffer, &cmdbegininfo) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkBeginCommandBuffer");
			return false;
		}

		vkCmdPipelineBarrier(stagingcommandbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stagingbuffertransferbarrier);
		vkCmdCopyBufferToImage(stagingcommandbuffer, stagingbuffer, texdata[i].teximg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &stagingbuffercopy);
		vkCmdPipelineBarrier(stagingcommandbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &stagingbuffershaderbarrier);


		if (vkEndCommandBuffer(stagingcommandbuffer) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkEndCommandBuffer");
			return false;
		}

		VkSubmitInfo submitinfo{};
		submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitinfo.pWaitDstStageMask = nullptr;
		submitinfo.waitSemaphoreCount = 0;
		submitinfo.pWaitSemaphores = nullptr;
		submitinfo.signalSemaphoreCount = 0;
		submitinfo.pSignalSemaphores = nullptr;
		submitinfo.commandBufferCount = 1;
		submitinfo.pCommandBuffers = &stagingcommandbuffer;

		VkFence stagingbufferfence;

		VkFenceCreateInfo fenceinfo{};
		fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;


		if (vkCreateFence(rdata.rdvkbdevice.device, &fenceinfo, nullptr, &stagingbufferfence) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkCreateFence");
			return false;
		}
		if (vkResetFences(rdata.rdvkbdevice.device, 1, &stagingbufferfence) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkResetFences");
			return false;
		}
		//if (vkWaitForFences(rdata.rdvkbdevice.device, 1, &rdata.rdrenderfence, VK_TRUE, INT64_MAX) != VK_SUCCESS) {
		//	return false;
		//}

		rdata.mtx2->lock();
		if (vkQueueSubmit(rdata.rdgraphicsqueue, 1, &submitinfo, stagingbufferfence) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkQueueSubmit");
			return false;
		}

		rdata.mtx2->unlock();

		if (vkWaitForFences(rdata.rdvkbdevice.device, 1, &stagingbufferfence, VK_TRUE, INT64_MAX) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkWaitForFences");
			return false;
		}




		vkDestroyFence(rdata.rdvkbdevice.device, stagingbufferfence, nullptr);
		commandbuffer::cleanup(rdata, rdata.rdcommandpool[2], stagingcommandbuffer);
		vmaDestroyBuffer(rdata.rdallocator, stagingbuffer, stagingbufferalloc);

		VkImageViewCreateInfo texviewinfo{};
		texviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		texviewinfo.image = texdata[i].teximg;
		texviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		texviewinfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		texviewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		texviewinfo.subresourceRange.baseMipLevel = 0;
		texviewinfo.subresourceRange.levelCount = 1;
		texviewinfo.subresourceRange.baseArrayLayer = 0;
		texviewinfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(rdata.rdvkbdevice.device, &texviewinfo, nullptr, &texdata[i].teximgview) != VK_SUCCESS)
		{
			logger::log(0, "crashed in texture at vkCreateImageView");
			return false;
		}

		VkSamplerCreateInfo texsamplerinfo{};
		texsamplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		texsamplerinfo.magFilter = VK_FILTER_LINEAR;
		texsamplerinfo.minFilter = VK_FILTER_LINEAR;
		texsamplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		texsamplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		texsamplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		texsamplerinfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		texsamplerinfo.unnormalizedCoordinates = VK_FALSE;
		texsamplerinfo.compareEnable = VK_FALSE;
		texsamplerinfo.compareOp = VK_COMPARE_OP_ALWAYS;
		texsamplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		texsamplerinfo.mipLodBias = 0.0f;
		texsamplerinfo.minLod = 0.0f;
		texsamplerinfo.maxLod = 0.0f;
		texsamplerinfo.anisotropyEnable = VK_FALSE;
		texsamplerinfo.maxAnisotropy = 1.0f;


		if (vkCreateSampler(rdata.rdvkbdevice.device, &texsamplerinfo, nullptr, &texdata[i].texsampler) != VK_SUCCESS) {
			logger::log(0, "crashed in texture at vkCreateSampler");
			return false;
		}

	}

	return true;
}
bool vktexture::loadtexlayoutpool(vkobjs& rdata, std::vector<vktexdata>& texdata, vktexdatapls& texdatapls, fastgltf::Asset& mmodel) {

	VkDescriptorSetLayoutBinding texturebind;
	std::vector<VkDescriptorImageInfo> descriptorimginfo;
	descriptorimginfo.reserve(mmodel.images.size());
	descriptorimginfo.resize(mmodel.images.size());

	texturebind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturebind.binding = 0;
	texturebind.descriptorCount = mmodel.images.size();
	texturebind.pImmutableSamplers = nullptr;
	texturebind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	for (int i{ 0 }; i < mmodel.images.size(); i++) {

		descriptorimginfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorimginfo[i].imageView = texdata[i].teximgview;
		descriptorimginfo[i].sampler = texdata[i].texsampler;
	}



	VkDescriptorSetLayoutCreateInfo texcreateinfo{};
	texcreateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	texcreateinfo.bindingCount = 1;//mmodel->images.size();
	texcreateinfo.pBindings = &texturebind;

	if (vkCreateDescriptorSetLayout(rdata.rdvkbdevice.device, &texcreateinfo, nullptr, &texdatapls.texdescriptorlayout) != VK_SUCCESS) {
		logger::log(0, "crashed in texture at vkCreateDescriptorSetLayout");
		return false;
	}


	VkDescriptorPoolSize poolsize{};
	poolsize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolsize.descriptorCount = mmodel.images.size() * 1024 * 1024 * 4;

	VkDescriptorPoolCreateInfo descriptorpool{};
	descriptorpool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorpool.poolSizeCount = 1;
	descriptorpool.pPoolSizes = &poolsize;
	descriptorpool.maxSets = 16;

	if (vkCreateDescriptorPool(rdata.rdvkbdevice.device, &descriptorpool, nullptr, &texdatapls.texdescriptorpool) != VK_SUCCESS) {
		logger::log(0, "crashed in texture at vkCreateDescriptorPool");
		return false;
	}


	VkDescriptorSetAllocateInfo descallocinfo{};
	descallocinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descallocinfo.descriptorPool = texdatapls.texdescriptorpool;
	descallocinfo.descriptorSetCount = 1;
	descallocinfo.pSetLayouts = &texdatapls.texdescriptorlayout;


	if (vkAllocateDescriptorSets(rdata.rdvkbdevice.device, &descallocinfo, &texdatapls.texdescriptorset) != VK_SUCCESS) {
		logger::log(0, "crashed in texture at vkAllocateDescriptorSets");
		return false;
	}

	VkWriteDescriptorSet writedescset{};
	writedescset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writedescset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writedescset.dstSet = texdatapls.texdescriptorset;
	writedescset.dstArrayElement = 0;
	writedescset.dstBinding = 0;
	//writedescset.pBufferInfo = 0;
	writedescset.descriptorCount = mmodel.images.size();
	writedescset.pImageInfo = descriptorimginfo.data();
	vkUpdateDescriptorSets(rdata.rdvkbdevice.device, 1, &writedescset, 0, nullptr);

	return true;
}


void vktexture::cleanup(vkobjs& rdata,vktexdata& texdata) {
	vkDestroySampler(rdata.rdvkbdevice.device, texdata.texsampler, nullptr);
	vkDestroyImageView(rdata.rdvkbdevice.device, texdata.teximgview, nullptr);
	vmaDestroyImage(rdata.rdallocator, texdata.teximg, texdata.teximgalloc);
}

void vktexture::cleanuppls(vkobjs& rdata, vktexdatapls& texdatapls){
	vkDestroyDescriptorPool(rdata.rdvkbdevice.device, texdatapls.texdescriptorpool, nullptr);
	vkDestroyDescriptorSetLayout(rdata.rdvkbdevice.device, texdatapls.texdescriptorlayout, nullptr);
}
