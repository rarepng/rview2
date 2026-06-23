#pragma once
#include <vulkan/vulkan.h>
#include <string_view>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

namespace rdebug {

constexpr bool is_active = RVIEW_DEBUG;

inline PFN_vkSetDebugUtilsObjectNameEXT SetObjectName = nullptr;
inline PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginLabel = nullptr;
inline PFN_vkCmdEndDebugUtilsLabelEXT CmdEndLabel   = nullptr;
inline TracyVkCtx tracyCTX   = nullptr;

inline void init(VkInstance instance, VkPhysicalDevice physdev, VkDevice device, VkQueue queue, VkCommandBuffer cmdbuf) {
	if constexpr (is_active) {
		SetObjectName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
		CmdBeginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
		CmdEndLabel   = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
		tracyCTX = TracyVkContext(physdev, device, queue, cmdbuf);
	}
}

template<typename T>
inline void name_obj(VkDevice device, T handle, VkObjectType type, std::string_view name) {
	if constexpr (is_active) {
		if (SetObjectName && handle) {
			VkDebugUtilsObjectNameInfoEXT info{};
			info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
			info.objectType = type;
			info.objectHandle = reinterpret_cast<uint64_t>(handle);
			info.pObjectName = name.data();
			SetObjectName(device, &info);
		}
	}
}

inline void begin_pass(VkCommandBuffer cmd, std::string_view name) {
	if constexpr (is_active) {
		if (CmdBeginLabel) {
			VkDebugUtilsLabelEXT label{};
			label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
			label.pLabelName = name.data();
			label.color[0] = 1.0f;
			label.color[1] = 0.5f;
			label.color[2] = 0.0f;
			label.color[3] = 1.0f;
			CmdBeginLabel(cmd, &label);
		}
	}
}

inline void end_pass(VkCommandBuffer cmd) {
	if constexpr (is_active) {
		if (CmdEndLabel) CmdEndLabel(cmd);
	}
}
inline void set_thread_name([[maybe_unused]] const char* name) {
#ifdef TRACY_ENABLE
	tracy::SetThreadName(name);
#endif
}
}