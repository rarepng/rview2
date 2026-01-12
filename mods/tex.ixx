module; 
//DO NOT ADD ANY HEADER FILES HERE NOR import std;
#include <vector>
#include <functional> 
#include <string_view>
#include <cstdint>

export module rview.rvk.tex;

//this is the most ridiculous and confusing thing about modules
//wtf
extern "C++" {
    struct VkImage_T; using VkImage = VkImage_T*;
    struct VkImageView_T; using VkImageView = VkImageView_T*;
    struct VkSampler_T; using VkSampler = VkSampler_T*;
    struct VkDescriptorSetLayout_T; using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;
    struct VkDescriptorSet_T; using VkDescriptorSet = VkDescriptorSet_T*;
    struct VmaAllocation_T; using VmaAllocation = VmaAllocation_T*;
    struct VkDescriptorPool_T; using VkDescriptorPool = VkDescriptorPool_T*;
    

    namespace fastgltf { class Asset; }

    struct rvkbucket; 
    struct texdata; 
}

export namespace rview::rvk {
    
    using TextureErrorCallback = std::function<void(std::string_view)>;

    struct BatchResult {
        size_t success_count;
        bool complete_success;
    };
}

export namespace rview::rvk::tex {
    
    BatchResult load_batch(
        rvkbucket& rdata, 
        std::vector<texdata>& out_textures, 
        fastgltf::Asset& model,
        TextureErrorCallback on_error = nullptr
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
    void cleanuptpl(rvkbucket& rdata,    VkDescriptorSetLayout& layout,VkDescriptorPool& pool);
}