// #include "anim/vkanim.hpp"
// #include "vkclip.hpp" 
// #include "vknode.hpp"
// #include <vector>
// #include <execution>

// namespace AnimBaker {

//     // Output containers for the GPU upload
//     struct BakeResult {
//         std::vector<PackedTRS> stream;      // Upload to BDA Buffer 0
//         std::vector<BakedClipHeader> headers; // Upload to a mapped buffer or struct
//     };

//     // Helper: Flatten Hierarchy to Linear Array (DFS)
//     // Ensures Parent is always processed before Child in the Compute Shader
//     void FlattenHierarchy(std::shared_ptr<vknode> node, std::vector<int32_t>& parents, int32_t parentIdx) {
//         int32_t currentIdx = (int32_t)parents.size();
//         parents.push_back(parentIdx);
        
//         for (auto& child : node->getchildren()) {
//             FlattenHierarchy(child, parents, currentIdx);
//         }
//     }

//     // THE BAKE FUNCTION
//     // "Bakes" your high-precision Cubic Splines into a fast GPU Linear Stream.
//     BakeResult BakeClips(std::vector<std::shared_ptr<vkclip>>& clips, std::shared_ptr<vknode> root, float sampleRate = 30.0f) {
//         BakeResult result;
//         uint32_t currentStreamOffset = 0;

//         // 1. Pre-calculate Skeleton Hierarchy for indexing
//         std::vector<vknode*> linearNodes;
//         // ... (Implement simple DFS to fill linearNodes matching FlattenHierarchy order) ...
        
//         for (const auto& clip : clips) {
//             BakedClipHeader header{};
//             header.startOffset = currentStreamOffset;
//             header.duration = clip->getEndTime();
//             header.sampleRate = sampleRate;
//             header.frameCount = (uint32_t)(header.duration * sampleRate) + 1;

//             // Resize stream to fit this clip
//             size_t startSize = result.stream.size();
//             size_t addedSize = header.frameCount * linearNodes.size();
//             result.stream.resize(startSize + addedSize);

//             // Parallel Bake Loop: Iterate Frames
//             // We use your exact vkchannel logic here!
//             auto frameRange = std::views::iota(0u, header.frameCount);
//             std::for_each(std::execution::par, frameRange.begin(), frameRange.end(), [&](uint32_t f) {
//                 float time = f / sampleRate;
                
//                 // Base pointer for this frame in the stream
//                 size_t frameBase = startSize + (f * linearNodes.size());

//                 for (size_t b = 0; b < linearNodes.size(); ++b) {
//                     vknode* node = linearNodes[b];
                    
//                     // Reset to Bind Pose (Identity)
//                     glm::vec3 t = node->translation; // Assuming bind pose stored in node
//                     glm::quat r = node->rotation;
//                     glm::vec3 s = node->scale;

//                     // Apply Animation Channels (Using YOUR logic)
//                     // Note: In your code, you iterate channels. Here we need to find the channel for the node.
//                     // Optimization: Build a map<NodeID, vector<vkchannel*>> outside the loop.
                    
//                     // Simulate your blendFrame/setFrame logic:
//                     for (auto& chan : clip->animchannels) {
//                         if (chan->getTargetNode() == node->getID()) { // Assuming getID() exists or mapping
//                              switch (chan->getAnimType()) {
//                                 case animType::TRANSLATION: t = chan->getTranslate(time); break;
//                                 case animType::ROTATION:    r = chan->getRotate(time); break;
//                                 case animType::SCALE:       s = chan->getScale(time); break;
//                             }
//                         }
//                     }

//                     // Pack for GPU
//                     PackedTRS& trs = result.stream[frameBase + b];
//                     trs.t = t;
//                     trs.r = glm::vec4(r.x, r.y, r.z, r.w);
//                     trs.s = s;
//                 }
//             });

//             currentStreamOffset += addedSize;
//             result.headers.push_back(header);
//         }
//         return result;
//     }
// }