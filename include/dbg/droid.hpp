#pragma once

#include <core/rvk.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

extern "C" int __android_log_write(int prio, const char* tag, const char* text);

namespace rdroid {

	// dont ask me to explain this one X_X
	// same numbers as android_LogPriority
	enum class prio : int { verbose = 2, debug = 3, info = 4, warn = 5, error = 6, fatal = 7 };

	template <class... Args> void logcat(prio p, const char* tag, std::format_string<Args...> fmt, Args&&... args) {
		if constexpr (rview::core::platform::is_android) {
			std::array<char, 1024> buf;
			char* end = std::format_to_n(buf.data(), std::ssize(buf) - 1, fmt, std::forward<Args>(args)...).out;
			*end = '\0';
			__android_log_write(static_cast<int>(p), tag, buf.data());
		}
	}
	template <class... Args> void alog(std::format_string<Args...> fmt, Args&&... args) {
		logcat(prio::error, "VulkanDiag", fmt, std::forward<Args>(args)...);
	}

	inline void trace_step(std::string_view msg) {
		logcat(prio::error, "VkTrace", ">>> REACHED: {}", msg);
	}

	inline void diagnose_gpu(VkInstance instance) {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			alog("FATAL: Zero Vulkan physical devices found by the instance!");
			return;
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (VkPhysicalDevice dev : devices) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(dev, &props);

			alog("========================================");
			alog("INSPECTING GPU: {}", std::string_view{props.deviceName});
			alog("API VERSION: 1.{}.{}", VK_API_VERSION_MINOR(props.apiVersion), VK_API_VERSION_PATCH(props.apiVersion));

			VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
			VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
			VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
			VkPhysicalDeviceFeatures2 feats2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

			feats2.pNext = &f11;
			f11.pNext = &f12;
			f12.pNext = &f13;
			vkGetPhysicalDeviceFeatures2(dev, &feats2);

			if (!feats2.features.samplerAnisotropy)
				alog("[MISSING] samplerAnisotropy");
			if (!feats2.features.multiDrawIndirect)
				alog("[MISSING] multiDrawIndirect");
			if (!feats2.features.vertexPipelineStoresAndAtomics)
				alog("[MISSING] vertexPipelineStoresAndAtomics");
			if (!feats2.features.shaderInt16)
				alog("[MISSING] shaderInt16");
			if (!feats2.features.drawIndirectFirstInstance)
				alog("[MISSING] drawIndirectFirstInstance");

			if (!f11.shaderDrawParameters)
				alog("[MISSING] shaderDrawParameters");
			if (!f11.storageBuffer16BitAccess)
				alog("[MISSING] storageBuffer16BitAccess");
			if (!f11.uniformAndStorageBuffer16BitAccess)
				alog("[MISSING] uniformAndStorageBuffer16BitAccess");

			if (!f12.runtimeDescriptorArray)
				alog("[MISSING] runtimeDescriptorArray");
			if (!f12.storageBuffer8BitAccess)
				alog("[MISSING] storageBuffer8BitAccess");
			if (!f12.uniformAndStorageBuffer8BitAccess)
				alog("[MISSING] uniformAndStorageBuffer8BitAccess");
			if (!f12.shaderInt8)
				alog("[MISSING] shaderInt8");
			if (!f12.descriptorBindingPartiallyBound)
				alog("[MISSING] descriptorBindingPartiallyBound");
			if (!f12.descriptorIndexing)
				alog("[MISSING] descriptorIndexing");
			if (!f12.descriptorBindingStorageBufferUpdateAfterBind)
				alog("[MISSING] descriptorBindingStorageBufferUpdateAfterBind");
			if (!f12.descriptorBindingSampledImageUpdateAfterBind)
				alog("[MISSING] descriptorBindingSampledImageUpdateAfterBind");
			if (!f12.descriptorBindingVariableDescriptorCount)
				alog("[MISSING] descriptorBindingVariableDescriptorCount");
			if (!f12.drawIndirectCount)
				alog("[MISSING] drawIndirectCount");

			if (!f13.dynamicRendering)
				alog("[MISSING] dynamicRendering");
			if (!f13.synchronization2)
				alog("[MISSING] synchronization2");
			if (!f13.shaderDemoteToHelperInvocation)
				alog("[MISSING] shaderDemoteToHelperInvocation");

			alog("========================================");
		}
	}

}