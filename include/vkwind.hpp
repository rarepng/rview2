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

#include <fstream>
#include <dbg/droid.hpp>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <dbg/trace.hpp>


static inline VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														   VkDebugUtilsMessageTypeFlagsEXT messageType,
														   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
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
	rdroid::logcat(rdroid::prio::error, "rview2", "\n [rview2] [VALIDATION ERROR]:\n {} \n", pCallbackData->pMessage);
	SDL_Log("\n [rview2] [VALIDATION ERROR]:\n %s \n", pCallbackData->pMessage);

	std::cerr << "\n[VALIDATION ERROR]: " << pCallbackData->pMessage << "\n" << std::endl;

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
		// f12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
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
		f11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
		VkPhysicalDeviceFeatures features{};
		features.samplerAnisotropy = VK_TRUE;
		features.multiDrawIndirect = VK_TRUE;
		features.vertexPipelineStoresAndAtomics = VK_TRUE;
		features.shaderInt16 = VK_TRUE;
		features.drawIndirectFirstInstance = VK_TRUE;
		features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
		features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
		// VkPhysicalDeviceMeshShaderFeaturesEXT f_mesh{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
		// f_mesh.meshShader = VK_TRUE;
		// f_mesh.taskShader = VK_TRUE;
		// VkPhysicalDeviceRayQueryFeaturesKHR ray_query_feats{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
		// ray_query_feats.rayQuery = VK_TRUE;
		// VkPhysicalDeviceAccelerationStructureFeaturesKHR as_feats{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
		// as_feats.accelerationStructure = VK_TRUE;
		// VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT dynamicVertexFeats{
		// 	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT,
		// 	.vertexInputDynamicState = VK_TRUE
		// };

		if constexpr (!rview::core::platform::is_android) {
			// DESKTOP ONLY: Allow 8/16 bit in Uniform Buffers
			f12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
			f11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
		} else {
			f12.uniformAndStorageBuffer8BitAccess = VK_FALSE;
			f11.uniformAndStorageBuffer16BitAccess = VK_FALSE;
		}

		selector.set_minimum_version(1, rview::core::platform::is_android ? 3 : 4)
			.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
			// .set_required_features_14(f14)
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
	inline std::expected<vkb::PhysicalDevice, std::string> find_capable_gpu(rvkbucket& mvkobjs) noexcept {

		vkb::InstanceBuilder instbuild{};
		instbuild.set_app_name("rview2").require_api_version(1, rview::core::platform::is_android ? 3 : 4);

		if constexpr (rdebug::is_active) {
			instbuild.request_validation_layers(true).use_default_debug_messenger().set_debug_callback(debugCallback);
		}

		Uint32 ext_count = 0;
		char const* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);
		if (sdl_exts) {
			for (Uint32 i = 0; i < ext_count; ++i) {
				instbuild.enable_extension(sdl_exts[i]);
			}
		}

		auto instret = instbuild.build();

		if (!instret)
			return std::unexpected(instret.error().message());

		mvkobjs.inst = instret.value();

		// this entire block is for only anroid
		if constexpr (rview::core::platform::is_android) {
			uint32_t gpu_count = 0;
			vkEnumeratePhysicalDevices(mvkobjs.inst, &gpu_count, nullptr);
			std::vector<VkPhysicalDevice> gpus(gpu_count);
			vkEnumeratePhysicalDevices(mvkobjs.inst, &gpu_count, gpus.data());

			char* pref_path = SDL_GetPrefPath("red", "rview2");
			if (pref_path) {
				std::string log_file = std::string(pref_path) + "vk_crash_log.txt";
				std::ofstream log(log_file, std::ios::trunc);
				log << "Found " << gpu_count << " native GPUs.\n";

				for (auto gpu : gpus) {
					VkPhysicalDeviceProperties props;
					vkGetPhysicalDeviceProperties(gpu, &props);
					log << "============================\n";
					log << "NATIVE GPU: " << props.deviceName << "\n";
					log << "API VERSION: 1." << VK_API_VERSION_MINOR(props.apiVersion) << "\n";

					if (VK_API_VERSION_MINOR(props.apiVersion) < 3) {
						log << "FAILURE REASON: App requires Vulkan 1.3, but hardware only supports 1." << VK_API_VERSION_MINOR(props.apiVersion)
							<< "\n";
					}
				}
				SDL_free(pref_path);
			}

			rdroid::diagnose_gpu(mvkobjs.inst);
		}

		vkb::PhysicalDeviceSelector selector{mvkobjs.inst};

		configure_selector(selector);

		auto phys_ret = selector.require_present(false).select();

		if constexpr (rdebug::is_active) {
			if (!phys_ret) {
				std::cerr << "[Debug] find_capable_gpu FAILED: " << phys_ret.error().message() << std::endl;
			} else {
				std::cout << "[Debug] find_capable_gpu SELECTED: " << phys_ret.value().name << std::endl;
			}
		}

		if (!phys_ret)
			return std::unexpected(phys_ret.error().message());

		return phys_ret.value();
	}
	inline std::expected<rctx, std::string> finalize_device(rvkbucket& mvkobjs, vkb::PhysicalDevice candidate_gpu) noexcept {
		rdroid::trace_step("finalize_device: Configuring selector");
		vkb::PhysicalDeviceSelector selector{mvkobjs.inst};
		configure_selector(selector);

		rdroid::trace_step("finalize_device: re-selecting with surface");
		auto final_phys_ret = selector.set_surface(mvkobjs.surface).select();

		if (!final_phys_ret) {
			rdroid::trace_step("finalize_device: re-selection failed, using fallback GPU");
		} else {
			rdroid::trace_step("finalize_device: re-selection succeeded");
			candidate_gpu = final_phys_ret.value();
		}

		rdroid::trace_step("finalize_device: Building Device");
		vkb::DeviceBuilder dev_builder{candidate_gpu};
		auto dev_ret = dev_builder.build();

		if (!dev_ret) {
			rdroid::trace_step("finalize_device: dev_builder.build() FAILED");
			return std::unexpected(dev_ret.error().message());
		}

		rdroid::trace_step("finalize_device: Device built successfully");
		mvkobjs.vkdevice = dev_ret.value();

		rctx ctx;
		return ctx;
	}


	inline bool init(std::string title, rvkbucket& mvkobjs) {
		rdroid::trace_step("Entering init()");
		rdroid::logcat(rdroid::prio::error, "rview2", "!!!!!!!!!!!!!!!! RVIEW2 PRIMED !!!!!!!!!!!!!!!!");


		if (!SDL_Init(SDL_INIT_VIDEO)) {
			rdroid::trace_step("SDL_Init FAILED");
			return false;
		}
		rdroid::trace_step("SDL_Init SUCCESS");

		mvkobjs.rdmode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());

		rdroid::trace_step("Calling find_capable_gpu()");
		auto candidate_ret = find_capable_gpu(mvkobjs);

		if (!candidate_ret) {
			rdroid::trace_step("find_capable_gpu FAILED - returning false");
			return false;
		}
		rdroid::trace_step("find_capable_gpu SUCCESS");

		rdroid::trace_step("Calling SDL_CreateWindow");
		mvkobjs.wind = SDL_CreateWindow(title.c_str(), 900, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

		if (!mvkobjs.wind) {
			rdroid::trace_step("SDL_CreateWindow FAILED");
			SDL_Quit();
			return false;
		}
		rdroid::trace_step("SDL_CreateWindow SUCCESS");

		rdroid::trace_step("Calling SDL_Vulkan_CreateSurface");
		if (!SDL_Vulkan_CreateSurface(mvkobjs.wind, mvkobjs.inst, nullptr, &mvkobjs.surface)) {
			rdroid::trace_step("SDL_Vulkan_CreateSurface FAILED");
			return false;
		}
		rdroid::trace_step("SDL_Vulkan_CreateSurface SUCCESS");

		rdroid::trace_step("Calling finalize_device");
		auto ctx = finalize_device(mvkobjs, candidate_ret.value());

		if (!ctx.has_value()) {
			rdroid::trace_step("finalize_device FAILED");
			return false;
		}
		rdroid::trace_step("finalize_device SUCCESS");
		mvkobjs.ctx = ctx.value();

		if constexpr (!rview::core::platform::is_android) {
			SDL_SetCursor(SDL_CreateColorCursor(IMG_Load("resources/mouser.png"), 0, 0));
			SDL_SetWindowIcon(mvkobjs.wind, IMG_Load("resources/icon0.png"));
		}

		rdroid::trace_step("Calling vkrenderer::init");
		if (!vkrenderer::init(mvkobjs)) {
			rdroid::trace_step("vkrenderer::init FAILED");
			SDL_Quit();
			return false;
		}
		rdroid::trace_step("vkrenderer::init SUCCESS");

		rdroid::trace_step("Calling vkrenderer::setsize");
		vkrenderer::setsize(mvkobjs, 900, 600);

		rdroid::trace_step("init() FULLY COMPLETE");
		return true;
	}

	inline void frameupdate(rvkbucket& mvkobjs) {
		if (mvkobjs.mshutdown)
			return;

		vkrenderer::initscene(mvkobjs);
		vkrenderer::immediate_submit(mvkobjs, [&](VkCommandBuffer cbuffer) {
			// nothing now
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

			if (dt > 0.25)
				dt = 0.25;

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
						rview::core::cpu_relax();
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