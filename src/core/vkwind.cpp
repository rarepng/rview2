#include "vkwind.hpp"
#include <future>
#include <iostream>
#include <thread>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <expected>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {

	std::cerr << "\n[VALIDATION ERROR]: " << pCallbackData->pMessage << "\n"
	          << std::endl;

	return VK_FALSE;
}

// i think i must explicitly define everything required by the renderer to make sure it runs on all the suitable devices, right?
void configure_selector(vkb::PhysicalDeviceSelector& selector) {

	VkPhysicalDeviceVulkan14Features f14{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
	// f14.dynamicRenderingLocalRead = VK_TRUE;
	// f14.globalPriorityQuery = VK_TRUE;
	// f14.shaderSubgroupRotate = VK_TRUE;
	// f14.pushDescriptor = VK_TRUE;
	// f14.maintenance5 = VK_TRUE;
	// f14.maintenance6 = VK_TRUE;
	VkPhysicalDeviceVulkan13Features f13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	f13.dynamicRendering = VK_TRUE;
	f13.synchronization2 = VK_TRUE;
	f13.shaderDemoteToHelperInvocation = VK_TRUE;
	// f13.maintenance4 = VK_TRUE;
	// f13.computeFullSubgroups = VK_TRUE;
	// f13.subgroupSizeControl = VK_TRUE;
	VkPhysicalDeviceVulkan12Features f12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	f12.bufferDeviceAddress = VK_TRUE;
	f12.runtimeDescriptorArray = VK_TRUE;
	f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	f12.descriptorBindingPartiallyBound = VK_TRUE;
	f12.shaderFloat16 = VK_TRUE;
	// f12.descriptorIndexing = VK_TRUE;
	// f12.scalarBlockLayout = VK_TRUE;
	// f12.timelineSemaphore = VK_TRUE;
	// f12.hostQueryReset = VK_TRUE;
	// f12.shaderInt8 = VK_TRUE;
	// f12.bufferDeviceAddressCaptureReplay = VK_TRUE;
	// f12.drawIndirectCount = VK_TRUE;
	// f12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	// f12.shaderSubgroupExtendedTypes = VK_TRUE;
	// f12.samplerFilterMinmax = VK_TRUE;
	VkPhysicalDeviceVulkan11Features f11{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    // f11.storageBuffer16BitAccess = VK_TRUE;
	// f11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    // f11.shaderDrawParameters = VK_TRUE;
	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = VK_TRUE;
	features.vertexPipelineStoresAndAtomics = VK_TRUE;
	// features.shaderInt64 = VK_TRUE;
	// features.fragmentStoresAndAtomics = VK_TRUE;
	// features.multiDrawIndirect = VK_TRUE;
	// features.sparseBinding = VK_TRUE;
    // // features.sparseResidencyImage2D = VK_TRUE;
    // features.shaderImageGatherExtended = VK_TRUE;
	VkPhysicalDeviceMeshShaderFeaturesEXT f_mesh{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
	f_mesh.meshShader = VK_TRUE;
	f_mesh.taskShader = VK_TRUE;
	VkPhysicalDeviceRayQueryFeaturesKHR ray_query_feats{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
	ray_query_feats.rayQuery = VK_TRUE;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR as_feats{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
	as_feats.accelerationStructure = VK_TRUE;
	// as_feats.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtp_feats{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    // rtp_feats.rayTracingPipeline = VK_TRUE;
    // rtp_feats.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
	VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR bary_feats { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR };
    // bary_feats.fragmentShaderBarycentric = VK_TRUE;
	VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT atom_int64 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT };
    // // atom_int64.shaderImageInt64Atomics = VK_TRUE;
	VkPhysicalDeviceFragmentShadingRateFeaturesKHR vrs_feats{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR };
	// vrs_feats.pipelineFragmentShadingRate = VK_TRUE;
	// vrs_feats.primitiveFragmentShadingRate = VK_TRUE;
	// vrs_feats.attachmentFragmentShadingRate = VK_TRUE;
	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_float_feats{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT };
	// atomic_float_feats.shaderBufferFloat32Atomics = VK_TRUE;
	// atomic_float_feats.shaderImageFloat32Atomics = VK_TRUE;
	VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT atomic_float2_feats{ 
    	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT 
	};
	// atomic_float2_feats.shaderBufferFloat32AtomicMinMax = VK_TRUE; 
	// atomic_float2_feats.shaderSharedFloat32AtomicMinMax = VK_TRUE;
	// atomic_float2_feats.shaderImageFloat32AtomicMinMax  = VK_TRUE;
	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dyn_state3_feats{ 
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT 
	};
	// dyn_state3_feats.extendedDynamicState3PolygonMode = VK_TRUE;
	// dyn_state3_feats.extendedDynamicState3DepthClampEnable = VK_TRUE;
	// dyn_state3_feats.extendedDynamicState3LogicOpEnable = VK_TRUE;
	VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR rtp_maint1_feats{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR
	};
	// rtp_maint1_feats.rayTracingMaintenance1 = VK_TRUE;
	// rtp_maint1_feats.rayTracingPipelineTraceRaysIndirect2 = VK_TRUE;

	selector
	.set_minimum_version(1, 4)
	.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
	.set_required_features_14(f14)
	.set_required_features_13(f13)
	.set_required_features_12(f12)
	.set_required_features_11(f11)
	.set_required_features(features)
	.add_required_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME)
	.add_required_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME)
	.add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
	.add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
	.add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
	.add_required_extension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME)
	.add_required_extension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)
    .add_required_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME)
    .add_required_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME)
    .add_required_extension(VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME)
	// not sure
    // .add_required_extension(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME)
    // .add_required_extension(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME)
	// .add_required_extension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)
	// .add_required_extension(VK_NV_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME)
	// .add_required_extension(VK_EXT_SHADER_ATOMIC_FLOAT_2_EXTENSION_NAME)
	// .add_required_extension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME)
	// .add_required_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
	//need to add debug ones
	.add_required_extension_features(ray_query_feats)
	.add_required_extension_features(as_feats)
	.add_required_extension_features(f_mesh)
	.add_required_extension_features(rtp_feats)
	.add_required_extension_features(bary_feats)
	.add_required_extension_features(vrs_feats)
	.add_required_extension_features(atomic_float_feats)
    .add_required_extension_features(atom_int64)
	.add_required_extension_features(atomic_float2_feats)
    .add_required_extension_features(dyn_state3_feats)
    .add_required_extension_features(rtp_maint1_feats)
	.require_present(false);
}

[[nodiscard]]
std::expected<vkb::PhysicalDevice, std::string>
find_capable_gpu(rvkbucket& mvkobjs) noexcept {

	vkb::InstanceBuilder instbuild{};
	auto instret = instbuild
	               .set_app_name("rview2")
	               .require_api_version(1, 4)
#ifndef NDEBUG
	               .request_validation_layers(true)
	               .use_default_debug_messenger()
	               .set_debug_callback(debugCallback)
#endif
	               .build();

	if (!instret) return std::unexpected(instret.error().message());
	mvkobjs.inst = instret.value();

	vkb::PhysicalDeviceSelector selector{mvkobjs.inst};

	configure_selector(selector);

	auto phys_ret = selector
	                .require_present(false)
	                .select();
	if (!phys_ret) return std::unexpected(phys_ret.error().message());

	return phys_ret.value();
}
[[nodiscard]]
std::expected<rctx, std::string>
finalize_device(rvkbucket& mvkobjs, vkb::PhysicalDevice candidate_gpu) noexcept {

	vkb::PhysicalDeviceSelector selector{mvkobjs.inst};
	configure_selector(selector);
	auto final_phys_ret = selector
	                      .set_surface(mvkobjs.surface)
	                      .select();

	if (!final_phys_ret) {
		std::cerr << "[Warning] Could not re-select GPU with surface support. Using fallback." << std::endl;
	} else {
		candidate_gpu = final_phys_ret.value();
	}

	vkb::DeviceBuilder dev_builder{candidate_gpu};
	auto dev_ret = dev_builder.build();
	if (!dev_ret) return std::unexpected(dev_ret.error().message());

	mvkobjs.vkdevice = dev_ret.value();

	rctx ctx;
	ctx.cmdDrawMeshTasks = (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(mvkobjs.vkdevice, "vkCmdDrawMeshTasksEXT");

	return ctx;
}
bool vkwind::init(std::string title,rvkbucket& mvkobjs) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		return false;
	}
	mvkobjs.rdmode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());

	std::cout << SDL_GetError() << std::endl;

	auto candidate_ret = find_capable_gpu(mvkobjs);

	if (!candidate_ret) {
		std::cerr << "[Fatal] No suitable GPU found: " << candidate_ret.error() << std::endl;
		return false;
	}

	mvkobjs.wind = SDL_CreateWindow(title.c_str(), 900, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE); // SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT
	if (!mvkobjs.wind) {
		SDL_Quit();
		return false;
	}

	if (!SDL_Vulkan_CreateSurface(mvkobjs.wind, mvkobjs.inst, nullptr, &mvkobjs.surface)) {
		return false;
	}

	auto ctx = finalize_device(mvkobjs, candidate_ret.value());
	if (ctx.has_value()) {
		mvkobjs.ctx = ctx.value();
	} else {
		std::cerr << "[Fatal] Device finalization failed: " << ctx.error() << std::endl;
		return false;
	}

	SDL_SetCursor(SDL_CreateColorCursor(IMG_Load("resources/mouser.png"), 0, 0));
	SDL_SetWindowIcon(mvkobjs.wind, IMG_Load("resources/icon0.png"));

	if (!vkrenderer::init(mvkobjs)) {
		SDL_Quit();
		return false;
	}
	vkrenderer::setsize(mvkobjs,900, 600);

	return true;
}

void vkwind::frameupdate(rvkbucket& mvkobjs) {
	if (!mvkobjs.mshutdown) {
		vkrenderer::initscene(mvkobjs);
		vkrenderer::immediate_submit(mvkobjs,[&](VkCommandBuffer cbuffer) {
			vkrenderer::uploadfordraw(mvkobjs,cbuffer);
		});
		while (!mvkobjs.mshutdown) {
			if (!vkrenderer::draw(mvkobjs)) {
				break;
			}
		}
	}
}

void vkwind::cleanup(rvkbucket& mvkobjs) {
	vkrenderer::cleanup(mvkobjs);
	SDL_DestroyWindow(mvkobjs.wind);
	SDL_Quit();
}
