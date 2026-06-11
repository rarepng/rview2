#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace vkrenderer {
void* extract_vulkan_win32_handle(VkDevice device, VkDeviceMemory memory);
void* get_os_export_memory_info();
}

namespace obs_bridge {
constexpr bool use_d3d12 = false;

void* create_d3d_shared_texture(uint32_t width, uint32_t height, const uint8_t* device_luid);

void* get_vulkan_import_info(void* shared_handle);
void send_spout();
//     bool allocate_shared_windows_memory(
//         VkDevice device,
//         VkImage image,
//         uint32_t width,
//         uint32_t height,
//         const VkMemoryRequirements& reqs,
//         uint32_t memTypeIndex,
//         VkDeviceMemory* out_memory
//     );

void cleanup();
}