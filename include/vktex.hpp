#pragma once

#include "core/rvk.hpp"
#include <vector>
#include <functional>
#include <string_view>
#include <cstdint>

namespace fastgltf {
    class Asset;
    struct Image;
}

namespace rview::rvk {
    constexpr uint32_t MAX_GLOBAL_TEXTURES = 4096;

    struct TextureHeap {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
        uint32_t current_head = 0;

        [[nodiscard]] constexpr bool can_alloc(uint32_t count) const noexcept { 
            return current_head + count <= MAX_GLOBAL_TEXTURES; 
        }
    };

    void init_heap(rvkbucket& bucket, TextureHeap& heap);
    int32_t upload_texture(rvkbucket& bucket, TextureHeap& heap, fastgltf::Image& image, fastgltf::Asset& asset);
    void destroy_heap(rvkbucket& bucket, TextureHeap& heap);

    using TextureErrorCallback = std::function<void(std::string_view)>;

    struct BatchResult {
        size_t success_count;
        bool complete_success;
    };
}

namespace rview::rvk::tex {
    [[nodiscard]] BatchResult load_batch(
        rvkbucket& rdata,
        std::vector<texdata>& out_textures,
        fastgltf::Asset& model,
        rview::rvk::TextureErrorCallback on_error = nullptr
    );

    bool update_descriptor_set(
        rvkbucket& rdata,
        const std::vector<texdata>& textures,
        VkDescriptorSetLayout& layout,
        VkDescriptorPool& pool,
        VkDescriptorSet& target_set
    );

    void init_descriptors(rvkbucket& rdata);
    
    bool load_env_map(
        rvkbucket& rdata,
        texdata& out_env_map,
        VkDescriptorSet& target_set,
        VkDescriptorSetLayout& target_lay
    );

    void cleanup(rvkbucket& rdata, texdata& tex);
    void cleanuptpl(rvkbucket& rdata, VkDescriptorSetLayout& layout, VkDescriptorPool& pool);
}