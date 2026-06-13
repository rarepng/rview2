#pragma once
#include <ui.hpp>
#include <vkrenderer.hpp>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <memory>
#include <string>
#include <vulkan/vulkan.h>
#include <future>
#include <iostream>
#include <thread>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <immintrin.h>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <dbg/trace.hpp>


static inline VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void* pUserData) {
	// im just sick of seeing these personally
	if (strstr(pCallbackData->pMessage, "uses API version 1.3 which is older") != nullptr) {
		return VK_FALSE;
	}

	if (strstr(pCallbackData->pMessage, "duplicate of VK_LAYER_EOS_Overlay") != nullptr) {
		return VK_FALSE;
	}

	if (strstr(pCallbackData->pMessage, "ReShade64.dll\" with error 1114") != nullptr) {
		return VK_FALSE;
	}

	std::cerr << "\n[VALIDATION ERROR]: " << pCallbackData->pMessage << "\n"
	          << std::endl;

	return VK_FALSE;
}


namespace vkwind {

// i think i must explicitly define everything required by the renderer to make sure it runs on all the suitable devices, right?
inline void configure_selector(vkb::PhysicalDeviceSelector& selector) {

	VkPhysicalDeviceVulkan14Features f14{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
	VkPhysicalDeviceVulkan13Features f13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	f13.dynamicRendering = VK_TRUE;
	f13.synchronization2 = VK_TRUE;
	f13.shaderDemoteToHelperInvocation = VK_TRUE;
	VkPhysicalDeviceVulkan12Features f12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	// f12.bufferDeviceAddress = VK_TRUE;
	f12.runtimeDescriptorArray = VK_TRUE;
	// f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	f12.storageBuffer8BitAccess = VK_TRUE;
	f12.shaderInt8 = VK_TRUE;
	f12.descriptorBindingPartiallyBound = VK_TRUE;
	f12.descriptorIndexing = VK_TRUE;
	f12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	f12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	f12.drawIndirectCount = VK_TRUE;
	// f12.shaderFloat16 = VK_TRUE;
	VkPhysicalDeviceVulkan11Features f11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
	f11.shaderDrawParameters = VK_TRUE;
	f11.storageBuffer16BitAccess = VK_TRUE;
	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = VK_TRUE;
	features.multiDrawIndirect = VK_TRUE;
	features.vertexPipelineStoresAndAtomics = VK_TRUE;
	features.shaderInt16 = VK_TRUE;
	features.drawIndirectFirstInstance = VK_TRUE;
	VkPhysicalDeviceMeshShaderFeaturesEXT f_mesh{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
	f_mesh.meshShader = VK_TRUE;
	f_mesh.taskShader = VK_TRUE;
	VkPhysicalDeviceRayQueryFeaturesKHR ray_query_feats{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
	ray_query_feats.rayQuery = VK_TRUE;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR as_feats{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
	as_feats.accelerationStructure = VK_TRUE;
	// VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT dynamicVertexFeats{
	// 	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,
	// 	.vertexInputDynamicState = VK_TRUE
	// };

	selector
	.set_minimum_version(1, 4)
	.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
	.set_required_features_14(f14)
	.set_required_features_13(f13)
	.set_required_features_12(f12)
	.set_required_features_11(f11)
	.set_required_features(features)
	// .add_required_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME)
	// .add_required_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME)
	// .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
	// .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
	// .add_required_extension("VK_EXT_vertex_input_dynamic")
	// //need to add debug ones
	// .add_required_extension_features(ray_query_feats)
	// .add_required_extension_features(as_feats)
	// .add_required_extension_features(f_mesh)
	// .add_required_extension_features(dynamicVertexFeats)
	;

	if constexpr (rview::core::platform::is_windows) {
		selector.add_required_extension("VK_KHR_external_memory_win32");
	}

	selector.require_present(false);
}

[[nodiscard]]
inline std::expected<vkb::PhysicalDevice, std::string>
find_capable_gpu(rvkbucket& mvkobjs) noexcept {

	vkb::InstanceBuilder instbuild{};
	instbuild.set_app_name("rview2")
	         .require_api_version(1, 4);

	if constexpr (vkdebug::is_active) {
		instbuild.request_validation_layers(true)
		         .use_default_debug_messenger()
		         .set_debug_callback(debugCallback);
	}

	auto instret = instbuild.build();

	if (!instret) return std::unexpected(instret.error().message());

	mvkobjs.inst = instret.value();

	vkb::PhysicalDeviceSelector selector{mvkobjs.inst};

	configure_selector(selector);

	auto phys_ret = selector
	                .require_present(false)
	                .select();

	if (vkdebug::is_active) {
		if (!phys_ret) {
			std::cerr << "[Debug] find_capable_gpu FAILED: " << phys_ret.error().message() << std::endl;
		} else {
			std::cout << "[Debug] find_capable_gpu SELECTED: " << phys_ret.value().name << std::endl;
		}
	}

	if (!phys_ret) return std::unexpected(phys_ret.error().message());

	return phys_ret.value();
}
[[nodiscard]]
inline std::expected<rctx, std::string>
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
	// ctx.cmdDrawMeshTasks = (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(mvkobjs.vkdevice, "vkCmdDrawMeshTasksEXT");
	// ctx.cmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)vkGetDeviceProcAddr(mvkobjs.vkdevice, "vkCmdSetVertexInputEXT");

	return ctx;
}


inline bool init(std::string title, rvkbucket& mvkobjs) {
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

	// will put in json eventually
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

	// idk where i should store these hardcoded stuff strings
	SDL_SetCursor(SDL_CreateColorCursor(IMG_Load("resources/mouser.png"), 0, 0));
	SDL_SetWindowIcon(mvkobjs.wind, IMG_Load("resources/icon0.png"));

	if (!vkrenderer::init(mvkobjs)) {
		SDL_Quit();
		return false;
	}

	vkrenderer::setsize(mvkobjs, 900, 600);

	return true;
}

inline void frameupdate(rvkbucket& mvkobjs) {
	if (mvkobjs.mshutdown) return;

	vkrenderer::initscene(mvkobjs);
	vkrenderer::immediate_submit(mvkobjs, [&](VkCommandBuffer cbuffer) {
		vkrenderer::uploadfordraw(mvkobjs, cbuffer);
	});

	const double perf_freq = static_cast<double>(SDL_GetPerformanceFrequency());
	uint64_t last_counter = SDL_GetPerformanceCounter();
	double accumulator = 0.0;
	const double fixed_dt = 1.0 / 60.0;
	double target_fps = 144.0;
	double target_frame_time = target_fps > 0.0 ? (1.0 / target_fps) : 0.0;

	while (!mvkobjs.mshutdown) {
		uint64_t current_counter = SDL_GetPerformanceCounter();
		double dt = static_cast<double>(current_counter - last_counter) / perf_freq;
		last_counter = current_counter;

		if (dt > 0.25) dt = 0.25;

		accumulator += dt;

		while (accumulator >= fixed_dt) {
			accumulator -= fixed_dt;
		}

		if (!vkrenderer::draw(mvkobjs)) {
			break;
		}

		if (target_frame_time > 0.0) {
			uint64_t frame_end = SDL_GetPerformanceCounter();
			double elapsed_frame_time = static_cast<double>(frame_end - current_counter) / perf_freq;

			while (elapsed_frame_time < target_frame_time) {
				double remaining_time = target_frame_time - elapsed_frame_time;

				if (remaining_time > 0.002) {
					SDL_DelayNS(static_cast<Uint64>((remaining_time - 0.0015) * 1e9));
				} else {
					_mm_pause();
				}

				frame_end = SDL_GetPerformanceCounter();
				elapsed_frame_time = static_cast<double>(frame_end - current_counter) / perf_freq;
			}
		}

		FrameMark;
	}
}

inline void cleanup(rvkbucket& mvkobjs) {
	g_jobs.stop_and_join_all();
	vkDeviceWaitIdle(mvkobjs.vkdevice.device);
	vkrenderer::cleanup(mvkobjs);
	SDL_DestroyWindow(mvkobjs.wind);
	SDL_Quit();
}

};