#include <vulkan/vulkan.hpp>
#include "core/rvk.hpp"

struct ProbeGridInfo {
	float gridOriginX=0.0f, gridOriginY=0.0f, gridOriginZ=0.0f; // World space start position
	float voxelSize=50.0f;       // Distance between probes (e.g., 50.0 units)
	int   dimX=16, dimY=16, dimZ=16; // Number of probes (e.g., 16, 16, 16)
	int   maxProbeCount=4096;    // Safety cap (e.g., 4096)
};
struct IndirectCmd {
	uint32_t groupCountX = 0;
	uint32_t groupCountY = 1;
	uint32_t groupCountZ = 1;
};
[[nodiscard]] static VkImageCreateInfo make_image_info(uint32_t w, uint32_t h,VkFormat format) noexcept {
	return VkImageCreateInfo {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = { .width = w, .height = h, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
}
[[nodiscard]] static VkImageViewCreateInfo make_view_info(VkImage image, VkFormat format) noexcept {
	return VkImageViewCreateInfo {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.a = VK_COMPONENT_SWIZZLE_IDENTITY
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};
}
int gbufferimg(rvkbucket mvkobjs) {
	std::array<VkImage,3> gimgs{};
	std::array<VkImageView,3> gviews{};
	std::array<VmaAllocation,3> positionImageAlloc{};


	//hardcoded care
	std::array<VkImageCreateInfo,3> imginfos{
		make_image_info(900, 600,VK_FORMAT_R16G16B16A16_SFLOAT),
		make_image_info(900, 600,VK_FORMAT_R16G16B16A16_SFLOAT),
		make_image_info(900, 600,VK_FORMAT_R8G8B8A8_SRGB)
	};

	VmaAllocationCreateInfo alloc_info = { .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, .priority = 1.0f };
	for(size_t i{0}; i<gimgs.size(); i++) {
		vmaCreateImage(mvkobjs.alloc, &imginfos[i], &alloc_info, &gimgs[i], &positionImageAlloc[i], nullptr);
		VkImageViewCreateInfo viewinfos{make_view_info(gimgs[i], imginfos[i].format)};
		vkCreateImageView(mvkobjs.vkdevice.device, &viewinfos, nullptr, &gviews[i]);
	}
	return 0;

}

[[nodiscard]] int create_gbuffer_layout(VkDevice device, VkDescriptorSetLayout& out_layout) {
	// We need 4 bindings: Position(0), Normal(1), Albedo(2), Depth(3)
	// We use std::array for zero-overhead iteration
	std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

	for (uint32_t i = 0; i < 4; ++i) {
		bindings[i].binding = i; // 0, 1, 2, 3
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[i].descriptorCount = 1;
		// IMPORTANT: We want to use this in COMPUTE shaders (for lighting)
		// We also add FRAGMENT bit in case you want to debug it in a pixel shader later.
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[i].pImmutableSamplers = nullptr;
	}

	VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	return vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &out_layout) == VK_SUCCESS ? 0 : -1;
}
[[nodiscard]] int create_gbuffer_sampler(VkDevice device, VkSampler& out_sampler) {
	VkSamplerCreateInfo sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST, // No blending!
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
		.minLod = 0.0f,
		.maxLod = 1.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
	};

	return vkCreateSampler(device, &sampler_info, nullptr, &out_sampler) == VK_SUCCESS ? 0 : -1;
}


// [[nodiscard]] int create_acc_struct(rvkbucket mvkobjs){
//     // 1. The Vulkan-defined Instance struct
// VkAccelerationStructureInstanceKHR instance = {};

// // A. The Transform (Row Major 3x4 matrix)
// // [ 1 0 0 x ]
// // [ 0 1 0 y ]
// // [ 0 0 1 z ]
// instance.transform.matrix[0][0] = 1.0f; instance.transform.matrix[0][3] = worldPos.x;
// instance.transform.matrix[1][1] = 1.0f; instance.transform.matrix[1][3] = worldPos.y;
// instance.transform.matrix[2][2] = 1.0f; instance.transform.matrix[2][3] = worldPos.z;

// // B. The Mask (0xFF means "This object is visible to everyone")
// instance.mask = 0xFF;

// // C. The ID (Important!)
// // This 'instanceCustomIndex' is what we unpack in the shader later to know "This is a Chair"
// instance.instanceCustomIndex = objectID;

// // D. Flags (Usually NONE or TRIANGLE_CULL_DISABLE for double-sided)
// instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

// // E. The Reference (The GPU Address of the BLAS you already built)
// // You need to have built a BLAS for your mesh and queried its address.
// instance.accelerationStructureReference = blasDeviceAddress;

// // ---
// // Step: Upload an array of these structs to a Vulkan Buffer (usage: SHADER_DEVICE_ADDRESS | ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY)
// VkDeviceAddress instanceBufferAddr = getBufferDeviceAddress(instanceBuffer);




// // Define the geometry data
// VkAccelerationStructureGeometryKHR geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
// geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
// geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
// geometry.geometry.instances.arrayOfPointers = VK_FALSE; // We used a flat array, not pointers
// geometry.geometry.instances.data.deviceAddress = instanceBufferAddr; // Point to buffer from Phase A

// // Define the build info
// VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
// buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
// buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR; // Optimize for tracing speed
// buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR; // Use UPDATE if just moving things
// buildInfo.geometryCount = 1;
// buildInfo.pGeometries = &geometry;


// VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
// // "numInstances" is how many objects you have (e.g., 5000)
// vkGetAccelerationStructureBuildSizesKHR(
//     device,
//     VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
//     &buildInfo,
//     &numInstances,
//     &sizeInfo
// );

// // sizeInfo.accelerationStructureSize = Size required for the final TLAS
// // sizeInfo.buildScratchSize = Temporary memory needed during the build




// VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
// createInfo.buffer = tlasBuffer; // The buffer you just allocated
// createInfo.size = sizeInfo.accelerationStructureSize;
// createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

// vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &tlasHandle);




// // Update the build info to point to the new handles
// buildInfo.dstAccelerationStructure = tlasHandle;
// buildInfo.scratchData.deviceAddress = scratchBufferDeviceAddress;

// // The Range Info (How many instances to use)
// VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
// rangeInfo.primitiveCount = numInstances;
// rangeInfo.primitiveOffset = 0;
// rangeInfo.firstVertex = 0;
// rangeInfo.transformOffset = 0;
// const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

// // Record command
// vkCmdBuildAccelerationStructuresKHR(mvkobjs.cbuffers_compute[0], 1, &buildInfo, &pRangeInfo);

// // BARRIER: Critical!
// // You must wait for this build to finish before TraceProbe or any shader uses it.
// VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
// barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
// barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
// barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; // The Probe Trace shader
// barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

// vkCmdPipelineBarrier2(mvkobjs.cbuffers_compute[0], &barrier);




// // Update the build info to point to the new handles
// buildInfo.dstAccelerationStructure = tlasHandle;
// buildInfo.scratchData.deviceAddress = scratchBufferDeviceAddress;

// // The Range Info (How many instances to use)
// VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
// rangeInfo.primitiveCount = numInstances;
// rangeInfo.primitiveOffset = 0;
// rangeInfo.firstVertex = 0;
// rangeInfo.transformOffset = 0;
// const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

// // Record command
// vkCmdBuildAccelerationStructuresKHR(mvkobjs.cbuffers_compute[0], 1, &buildInfo, &pRangeInfo);

// // BARRIER: Critical!
// // You must wait for this build to finish before TraceProbe or any shader uses it.
// VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
// barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
// barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
// barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; // The Probe Trace shader
// barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

// vkCmdPipelineBarrier2(mvkobjs.cbuffers_compute[0], &barrier);
// }