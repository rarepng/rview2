
#include "core/rvk.hpp"
#include "fastgltf/types.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "fastgltf/tools.hpp"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

#include "genericmodel.hpp"
#include "vkvbo.hpp"

#include "vktex.hpp"

bool genericmodel::loadmodel(rvkbucket &objs, std::string_view fname) {

	fastgltf::Parser fastparser{};
	auto buff = fastgltf::MappedGltfFile::FromPath(fname);
	auto a =
	    fastparser.loadGltfBinary(buff.get(), std::filesystem::absolute(fname));
	mmodel2 = std::move(a.get());

	std::function<void(std::string_view)> error_handler =
	[](std::string_view msg) {
		std::cerr << std::format("tex err: {}\n", msg);
	};
	mgltfobjs.texs.reserve(mmodel2.images.size());
	mgltfobjs.texs.resize(mmodel2.images.size());
	auto result =
	    rview::rvk::tex::load_batch(objs, mgltfobjs.texs, mmodel2, error_handler);
	rview::rvk::tex::update_descriptor_set(objs, mgltfobjs.texs);
	// static const bool _ = [&] {
	// 	if (!vktexture::createlayout(objs))
	// 		return false;
	// 	return true;
	// }();

	// if (!vktexture::loadtexture(objs, mgltfobjs.texs, mmodel2))
	// 	return false;
	// if (!vktexture::loadtexset(objs, mgltfobjs.texs, *rvkbucket::texlayout,
	//                            mgltfobjs.dset, mmodel2))
	// 	return false;

	createvboebo(objs);

	AssetBaker::BakeSkeleton(mmodel2, flatskelly, 0);

	if (mmodel2.skins.size()) {
		getjointdata();
		getinvbindmats();

		mjnodecount = mmodel2.nodes.size();

	} else {
		skinned = false;
	}

	getanims();

	extractmaterials(objs);

	return true;
}

void genericmodel::extractmaterials(rvkbucket &objs) {
	std::vector<MaterialData> matBuffer;
	matBuffer.reserve(mmodel2.materials.size() + 1);

	for (const auto &mat : mmodel2.materials) {
		MaterialData md{};
		md.envMapMaxLod = rvkbucket::hdrmiplod - 1;
		md.baseColorFactor = glm::make_vec4(mat.pbrData.baseColorFactor.data());
		md.roughnessFactor = mat.pbrData.roughnessFactor;
		md.metallicFactor = mat.pbrData.metallicFactor;
		md.emissiveFactor = glm::make_vec3(mat.emissiveFactor.data());
		md.normalScale =
		    mat.normalTexture.has_value() ? mat.normalTexture.value().scale : 1.0f;

		if (mat.pbrData.baseColorTexture.has_value()) {
			size_t imgIdx = mmodel2.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
			md.albedoIdx = mgltfobjs.texs[imgIdx].bindless_idx;
		} else {
			md.albedoIdx = 1;
		}

		if (mat.normalTexture.has_value()) {
			size_t imgIdx = mmodel2.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
			md.normalIdx = mgltfobjs.texs[imgIdx].bindless_idx;
		} else {
			md.normalIdx = 2;
		}

		if (mat.pbrData.metallicRoughnessTexture.has_value()) {
			size_t imgIdx = mmodel2.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
			md.ormIdx = mgltfobjs.texs[imgIdx].bindless_idx;
		} else {
			md.ormIdx = 1;
		}

		if (mat.emissiveTexture.has_value()) {
			size_t imgIdx = mmodel2.textures[mat.emissiveTexture.value().textureIndex].imageIndex.value();
			md.emissiveIdx = mgltfobjs.texs[imgIdx].bindless_idx;
		} else {
			md.emissiveIdx = 3;
		}

		md.transmissionIdx = 3;
		md.sheenIdx = 3;
		md.clearcoatIdx = 3;
		md.thicknessIdx = 1;
		matBuffer.push_back(md);
	}

	MaterialData defaultMat{};
	defaultMat.baseColorFactor = glm::vec4(1.0f);
	defaultMat.roughnessFactor = 1.0f;
	defaultMat.metallicFactor = 0.0f;
	defaultMat.albedoIdx = 1;
	defaultMat.normalIdx = 2;
	defaultMat.ormIdx = 1;
	defaultMat.emissiveIdx = 3;
	defaultMat.transmissionIdx = 3;
	defaultMat.sheenIdx = 3;
	defaultMat.clearcoatIdx = 3;
	defaultMat.thicknessIdx = 1;
	defaultMat.envMapMaxLod = rvkbucket::hdrmiplod - 1;
	matBuffer.push_back(defaultMat);

	std::lock_guard<std::mutex> lock(rvkbucket::global_materials.mtx);

	if (rvkbucket::global_materials.free_slots.size() < matBuffer.size()) {
		std::cerr << "VULKAN ERROR: Global Material Heap Exhausted!" << std::endl;
		return;
	}

	MaterialData *global_array =
	    static_cast<MaterialData*>(rvkbucket::global_materials.mapped_data);

	for (size_t i = 0; i < matBuffer.size(); i++) {
		uint32_t global_slot = rvkbucket::global_materials.free_slots.front();
		rvkbucket::global_materials.free_slots.pop();

		m_global_material_indices.push_back(global_slot);

		global_array[global_slot] = matBuffer[i];
	}
}

int genericmodel::getnodecount() {
	return mjnodecount;
}

gltfnodedata genericmodel::getgltfnodes() {
	gltfnodedata nodeData{};

	if (mmodel2.skins.size()) {
		int rootNodeNum = mmodel2.scenes.at(0).nodeIndices.at(0);

		nodeData.rootnode = vknode::createroot(rootNodeNum);

		getnodedata(nodeData.rootnode);
		getnodes(nodeData.rootnode);

		nodeData.nodelist.reserve(mjnodecount);
		nodeData.nodelist.resize(mjnodecount);
		nodeData.nodelist.at(rootNodeNum) = nodeData.rootnode;
		getnodelist(nodeData.nodelist, rootNodeNum);
	}

	return nodeData;
}

void genericmodel::getjointdata() {
	mnodetojoint.reserve(mmodel2.nodes.size() * mmodel2.skins.size());
	mnodetojoint.resize(mmodel2.nodes.size() * mmodel2.skins.size());

	mnodetojoint.assign(mmodel2.nodes.size(), 0);
	unsigned int globalJointIndex = 0;

	for (const auto &skin : mmodel2.skins) {
		for (const size_t nodeIndex : skin.joints) {
			mnodetojoint.at(nodeIndex) = globalJointIndex++;
		}
	}
}

// void genericmodel::getinvbindmats() {

// 	minversebindmats.clear();
// 	mskinJointOffsets.clear();
// 	size_t currentOffset = 0;
// 	size_t totalJoints = 0;

// 	for (const auto& skin : mmodel2.skins) totalJoints +=
// skin.joints.size(); 	minversebindmats.reserve(totalJoints);

// 	for (const auto& skin : mmodel2.skins) {
// 		mskinJointOffsets.push_back(currentOffset);
// 		size_t jointCount = skin.joints.size();

// 		if (!skin.inverseBindMatrices.has_value()) {
// 			for (size_t i = 0; i < jointCount; ++i)
// minversebindmats.push_back(glm::mat4(1.0f)); 		} else { 			size_t
// invBindMatAccessor = skin.inverseBindMatrices.value(); 			const
// fastgltf::Accessor &accessor = mmodel2.accessors.at(invBindMatAccessor);
// 			const fastgltf::BufferView &bufferView =
// mmodel2.bufferViews.at(accessor.bufferViewIndex.value()); 			const
// fastgltf::Buffer &buffer = mmodel2.buffers.at(bufferView.bufferIndex);

// 			std::vector<glm::mat4> skinMats(jointCount);
// 			std::visit(fastgltf::visitor{
// 				[](auto &) {},
// 				[&](const fastgltf::sources::Array &vector) {
// 					std::memcpy(skinMats.data(),
// 					            vector.bytes.data() +
// bufferView.byteOffset + accessor.byteOffset, 					            jointCount * sizeof(glm::mat4));
// 				}
// 			}, buffer.data);

// 			minversebindmats.insert(minversebindmats.end(),
// skinMats.begin(), skinMats.end());
// 		}
// 		currentOffset += jointCount;
// 	}

// 	mmeshToSkinOffset.assign(mmodel2.meshes.size(), 0);
// 	for (const auto& node : mmodel2.nodes) {
// 		if (node.meshIndex.has_value() && node.skinIndex.has_value()) {
// 			mmeshToSkinOffset[node.meshIndex.value()] =
// mskinJointOffsets[node.skinIndex.value()];
// 		}
// 	}

// }
void genericmodel::getinvbindmats() {
	minversebindmats.clear();
	mskinJointOffsets.clear();
	size_t currentOffset = 0;
	size_t totalJoints = 0;

	for (const auto &skin : mmodel2.skins)
		totalJoints += skin.joints.size();

	minversebindmats.reserve(totalJoints);

	for (const auto &skin : mmodel2.skins) {
		mskinJointOffsets.push_back(currentOffset);
		size_t jointCount = skin.joints.size();

		if (!skin.inverseBindMatrices.has_value()) {
			for (size_t i = 0; i < jointCount; ++i)
				minversebindmats.push_back(glm::mat4(1.0f));
		} else {
			size_t invBindMatAccessor = skin.inverseBindMatrices.value();
			const fastgltf::Accessor &accessor =
			    mmodel2.accessors.at(invBindMatAccessor);

			std::vector<glm::mat4> skinMats(jointCount);
			fastgltf::copyFromAccessor<glm::mat4>(mmodel2, accessor, skinMats.data());

			minversebindmats.insert(minversebindmats.end(), skinMats.begin(),
			                        skinMats.end());
		}

		currentOffset += jointCount;
	}

	mmeshToSkinOffset.assign(mmodel2.meshes.size(), 0);

	for (const auto &node : mmodel2.nodes) {
		if (node.meshIndex.has_value() && node.skinIndex.has_value()) {
			mmeshToSkinOffset[node.meshIndex.value()] =
			    mskinJointOffsets[node.skinIndex.value()];
		}
	}
}

void genericmodel::getanims() {
	manimclips.reserve(mmodel2.animations.size());

	gltfnodedata nodedata = getgltfnodes();
	std::vector<bool> allTrueMask(nodedata.nodelist.size(), true);

	for (auto &anim0 : mmodel2.animations) {
		std::shared_ptr<vkclip> clip0 = std::make_shared<vkclip>(static_cast<std::string>(anim0.name));

		for (auto &c : anim0.channels) {
			clip0->addchan(mmodel2, anim0, c);
		}

		manimclips.push_back(clip0);

		DODAnimationClip baked{};
		baked.name = clip0->getName();
		baked.duration = clip0->getEndTime();
		baked.sampleRate = 12.0f;
		baked.nodeCount = flatskelly.nodeCount;
		baked.frameCount = static_cast<uint32_t>(baked.duration * baked.sampleRate) + 1;

		baked.localTransforms.resize(baked.frameCount * baked.nodeCount, glm::mat4(1.0f));

		for (uint32_t f = 0; f < baked.frameCount; ++f) {
			float t = static_cast<float>(f) / baked.sampleRate;
			clip0->setFrame(nodedata.nodelist, allTrueMask, t);

			uint32_t frameOffset = f * baked.nodeCount;

			for (uint32_t n = 0; n < nodedata.nodelist.size(); ++n) {
				if (n >= MAX_BONES) continue;

				int32_t topoIdx = flatskelly.gltfToTopoMap[n];

				if (topoIdx >= 0 && topoIdx < static_cast<int32_t>(baked.nodeCount)) {
					if (nodedata.nodelist[n]) {
						baked.localTransforms[frameOffset + topoIdx] = nodedata.nodelist[n]->getlocalmatrix();
					}
				}
			}
		}

		bakedClips.push_back(std::move(baked));
	}
}
std::vector<std::shared_ptr<vkclip>> genericmodel::getanimclips() {
	return manimclips;
}

void genericmodel::getnodes(std::shared_ptr<vknode> treeNode) {
	int nodeNum = treeNode->getnum();
	const auto &childNodes = mmodel2.nodes.at(nodeNum).children;

	treeNode->addchildren(childNodes);

	for (auto &childNode : treeNode->getchildren()) {
		getnodedata(childNode);
		getnodes(childNode);
	}
}

void genericmodel::getnodedata(std::shared_ptr<vknode> treeNode) {
	int nodeNum = treeNode->getnum();
	const fastgltf::Node &node = mmodel2.nodes.at(nodeNum);
	treeNode->setname(static_cast<std::string>(node.name));
	std::visit(fastgltf::visitor{
		[](auto & arg) {},
		[&](fastgltf::TRS trs) {
			treeNode->settranslation(
			    glm::make_vec3(trs.translation.data()));
			treeNode->setrotation(glm::make_quat(trs.rotation.data()));
			treeNode->setscale(glm::make_vec3(trs.scale.data()));
		}},
	node.transform);

	treeNode->calculatenodemat();
}

void genericmodel::resetnodedata(std::shared_ptr<vknode> treeNode) {
	getnodedata(treeNode);

	for (auto &childNode : treeNode->getchildren()) {
		resetnodedata(childNode);
	}
}

std::vector<std::shared_ptr<vknode>>
genericmodel::getnodelist(std::vector<std::shared_ptr<vknode>>& nodeList,
                          int nodeNum) {
	for (auto &childNode : nodeList.at(nodeNum)->getchildren()) {
		int childNodeNum = childNode->getnum();
		nodeList.at(childNodeNum) = childNode;
		getnodelist(nodeList, childNodeNum);
	}

	return nodeList;
}

std::vector<glm::mat4> genericmodel::getinversebindmats() {
	return minversebindmats;
}

std::vector<unsigned int> genericmodel::getnodetojoint() {
	return mnodetojoint;
}
void genericmodel::createvboebo(rvkbucket &objs) {
	size_t meshCount = mmodel2.meshes.size();
	mgltfobjs.vbos.resize(meshCount);
	mgltfobjs.ebos.resize(meshCount);
	meshjointtype.resize(meshCount);

	static const std::unordered_map<std::string_view, int> SLOT_MAP = {
		{"POSITION", 0},   {"NORMAL", 1},   {"TANGENT", 2},
		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
	};

	for (size_t i = 0; i < meshCount; ++i) {
		size_t primCount = mmodel2.meshes[i].primitives.size();

		mgltfobjs.vbos[i].resize(primCount);
		mgltfobjs.ebos[i].resize(primCount);
		meshjointtype[i].resize(primCount, false);

		for (size_t j = 0; j < primCount; ++j) {
			const auto &prim = mmodel2.meshes[i].primitives[j];

			if (prim.type != fastgltf::PrimitiveType::Triangles)
				continue;

			if (prim.indicesAccessor.has_value()) {
				const auto &idxAcc = mmodel2.accessors[prim.indicesAccessor.value()];
				size_t indexSize =
				    idxAcc.count *
				    fastgltf::getElementByteSize(idxAcc.type, idxAcc.componentType);

				vkvbo::init(objs, (GpuBuffer&)mgltfobjs.ebos[i][j], indexSize,
				            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
				            VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			}

			mgltfobjs.vbos[i][j].resize(6);

			bool hasJoints = false;
			bool hasWeights = false;
			size_t vertexCount = 0;

			const auto *posAttr = prim.findAttribute("POSITION");

			if (posAttr)
				vertexCount = mmodel2.accessors[posAttr->accessorIndex].count;

			auto init_attr = [&](const auto & attr) {
				auto it = SLOT_MAP.find(attr.name);

				if (it == SLOT_MAP.end()) return;

				int slot = it->second;

				const auto &acc = mmodel2.accessors[attr.accessorIndex];

				size_t size = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);

				if (slot == 4) {
					hasJoints = true;
					meshjointtype[i][j] = true;
				}

				if (slot == 5) {
					hasWeights = true;
				}

				vkvbo::init(objs, (GpuBuffer&)mgltfobjs.vbos[i][j][slot], size,
				            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

				uint32_t bIdx = rvkbucket::globalBufferCounter.fetch_add(1, std::memory_order_relaxed);
				mgltfobjs.vbos[i][j][slot].bindless_idx = bIdx;

				if (bIdx != 0) {
					VkDescriptorBufferInfo ssboInfo{mgltfobjs.vbos[i][j][slot].buffer, 0, VK_WHOLE_SIZE};
					VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
					write.dstSet = rvkbucket::globalBindlessSet;
					write.dstBinding = 5;
					write.dstArrayElement = bIdx;
					write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					write.descriptorCount = 1;
					write.pBufferInfo = &ssboInfo;

					std::lock_guard<std::shared_mutex> lock(*objs.mtx2);
					vkUpdateDescriptorSets(objs.vkdevice.device, 1, &write, 0, nullptr);
				}
			};

			std::ranges::for_each(prim.attributes, init_attr);

			// if (vertexCount > 0) {
			//   if (!hasJoints) {
			//     size_t size = vertexCount * sizeof(uint8_t) * 4;
			//     vkvbo::init(objs, (GpuBuffer &)mgltfobjs.vbos[i][j][4], size,
			//                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			//                     VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			//   }

			//   if (!hasWeights) {
			//     size_t size = vertexCount * sizeof(float) * 4;
			//     vkvbo::init(objs, (GpuBuffer &)mgltfobjs.vbos[i][j][5], size,
			//                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			//                     VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			//   }
			// }
		}
	}
}
const std::byte* getRawData(const fastgltf::DataSource &source) {
	return std::visit(
	[](auto & arg) -> const std::byte * {
		using T = std::decay_t<decltype(arg)>;

		if constexpr (requires { arg.bytes.data(); }) {
			return arg.bytes.data();
		}
		return nullptr;
	},
	source);
}
void genericmodel::uploadvboebo(rvkbucket &objs, VkCommandBuffer &cbuffer) {
	static const std::unordered_map<std::string_view, int> SLOT_MAP = {
		{"POSITION", 0},   {"NORMAL", 1},   {"TANGENT", 2},
		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
	};
	uint8_t* beltPtr = objs.sbelt.mappedData;
	VkBuffer beltBuffer = objs.sbelt.buffer;
	VkDeviceSize &offset = objs.sbelt.currentOffset;

	std::vector<VkBufferMemoryBarrier2> barriers;

	auto add_barrier = [&](VkBuffer buffer) {
		VkBufferMemoryBarrier2 b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
		b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT;
		b.buffer = buffer;
		b.size = VK_WHOLE_SIZE;
		barriers.push_back(b);
	};

	for (size_t i = 0; i < mmodel2.meshes.size(); ++i) {
		for (size_t j = 0; j < mmodel2.meshes[i].primitives.size(); ++j) {
			const auto &prim = mmodel2.meshes[i].primitives[j];

			if (prim.type != fastgltf::PrimitiveType::Triangles)
				continue;

			if (prim.indicesAccessor.has_value()) {
				const auto &acc = mmodel2.accessors[prim.indicesAccessor.value()];
				size_t s = acc.count *
				           fastgltf::getElementByteSize(acc.type, acc.componentType);

				if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
					fastgltf::copyFromAccessor<uint16_t>(mmodel2, acc, beltPtr + offset);
				} else if (acc.componentType == fastgltf::ComponentType::UnsignedInt) {
					fastgltf::copyFromAccessor<uint32_t>(mmodel2, acc, beltPtr + offset);
				} else if (acc.componentType == fastgltf::ComponentType::UnsignedByte) {
					fastgltf::copyFromAccessor<uint8_t>(mmodel2, acc, beltPtr + offset);
				}

				vmaFlushAllocation(objs.alloc, objs.sbelt.allocation, offset, s);

				VkBufferCopy region{offset, 0, s};
				vkCmdCopyBuffer(cbuffer, beltBuffer, mgltfobjs.ebos[i][j].buffer, 1,
				                &region);
				add_barrier(mgltfobjs.ebos[i][j].buffer);

				offset = (offset + s + 15) & ~15;
			}

			bool hasJoints = false;
			bool hasWeights = false;
			size_t vertexCount = 0;

			if (auto *p = prim.findAttribute("POSITION"))
				vertexCount = mmodel2.accessors[p->accessorIndex].count;

			auto upload_attr = [&](const auto & attr) {
				auto it = SLOT_MAP.find(attr.name);

				if (it == SLOT_MAP.end()) return;

				int slot = it->second;

				const auto &acc = mmodel2.accessors[attr.accessorIndex];
				size_t s = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);

				if (slot == 0 || slot == 1) {
					fastgltf::copyFromAccessor<glm::vec3>(mmodel2, acc, beltPtr + offset);
				} else if (slot == 2) {
					fastgltf::copyFromAccessor<glm::vec4>(mmodel2, acc, beltPtr + offset);
				} else if (slot == 3) {
					fastgltf::copyFromAccessor<glm::vec2>(mmodel2, acc, beltPtr + offset);
				} else if (slot == 4) {
					if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
						fastgltf::copyFromAccessor<glm::u16vec4>(mmodel2, acc, beltPtr + offset);
					} else if (acc.componentType == fastgltf::ComponentType::UnsignedByte) {
						fastgltf::copyFromAccessor<glm::u8vec4>(mmodel2, acc, beltPtr + offset);
					} else {
						fastgltf::copyFromAccessor<glm::u32vec4>(mmodel2, acc, beltPtr + offset);
					}
				} else if (slot == 5) {
					if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
						fastgltf::copyFromAccessor<glm::u16vec4>(mmodel2, acc, beltPtr + offset);
					} else if (acc.componentType == fastgltf::ComponentType::UnsignedByte) {
						fastgltf::copyFromAccessor<glm::u8vec4>(mmodel2, acc, beltPtr + offset);
					} else {
						fastgltf::copyFromAccessor<glm::vec4>(mmodel2, acc, beltPtr + offset);
					}
				}

				vmaFlushAllocation(objs.alloc, objs.sbelt.allocation, offset, s);

				VkBufferCopy region{offset, 0, s};
				vkCmdCopyBuffer(cbuffer, beltBuffer, mgltfobjs.vbos[i][j][slot].buffer, 1, &region);
				add_barrier(mgltfobjs.vbos[i][j][slot].buffer);

				offset = (offset + s + 15) & ~15;
			};
			std::ranges::for_each(prim.attributes, upload_attr);
		}
	}

	if (!barriers.empty()) {
		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
		dep.pBufferMemoryBarriers = barriers.data();
		vkCmdPipelineBarrier2(cbuffer, &dep);
	}
}
// void genericmodel::uploadvboebo(rvkbucket &objs, VkCommandBuffer &cbuffer) {
// 	static const std::unordered_map<std::string_view, int> SLOT_MAP = {
// 		{"POSITION", 0}, {"NORMAL", 1}, {"TANGENT", 2},
// 		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
// 	};
// 	uint8_t* beltPtr = objs.sbelt.mappedData;
// 	VkBuffer beltBuffer = objs.sbelt.buffer;
// 	VkDeviceSize& offset = objs.sbelt.currentOffset;

// 	std::vector<VkBufferMemoryBarrier2> barriers;

// 	// maybe will move this to rvk or a new namespace
// 	auto add_barrier = [&](VkBuffer buffer) {
// 		VkBufferMemoryBarrier2
// b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}; 		b.srcStageMask =
// VK_PIPELINE_STAGE_2_TRANSFER_BIT; 		b.srcAccessMask =
// VK_ACCESS_2_TRANSFER_WRITE_BIT; 		b.dstStageMask =
// VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
// 		b.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT |
// VK_ACCESS_2_INDEX_READ_BIT; 		b.buffer = buffer; 		b.size = VK_WHOLE_SIZE;
// 		barriers.push_back(b);
// 	};

// 	for (size_t i = 0; i < mmodel2.meshes.size(); ++i) {
// 		for (size_t j = 0; j < mmodel2.meshes[i].primitives.size(); ++j)
// { 			const auto& prim = mmodel2.meshes[i].primitives[j]; 			if (prim.type !=
// fastgltf::PrimitiveType::Triangles) continue;

// 			if (prim.indicesAccessor.has_value()) {
// 				const auto& acc =
// mmodel2.accessors[prim.indicesAccessor.value()]; 				const auto& view =
// mmodel2.bufferViews[acc.bufferViewIndex.value()]; 				const auto& bin =
// mmodel2.buffers[view.bufferIndex];

// 				vkvbo::record_upload(objs, cbuffer,
// (GpuBuffer&)mgltfobjs.ebos[i][j], 				                     bin, view, acc, beltPtr, beltBuffer,
// offset); 				add_barrier(mgltfobjs.ebos[i][j].buffer);

// 				size_t s = acc.count *
// fastgltf::getElementByteSize(acc.type, acc.componentType); 				offset = (offset +
// s + 15) & ~15;
// 			}

// 			bool hasJoints = false;
// 			bool hasWeights = false;
// 			size_t vertexCount = 0;
// 			if (auto* p = prim.findAttribute("POSITION"))
// vertexCount = mmodel2.accessors[p->accessorIndex].count;

// 			auto upload_attr = [&](const auto& attr) {
// 				auto it = SLOT_MAP.find(attr.name);
// 				if (it == SLOT_MAP.end()) return;
// 				int slot = it->second;

// 				const auto& acc =
// mmodel2.accessors[attr.accessorIndex]; 				const auto& view =
// mmodel2.bufferViews[acc.bufferViewIndex.value()]; 				const auto& bin =
// mmodel2.buffers[view.bufferIndex];

// 				if (slot == 4) hasJoints = true;
// 				if (slot == 5) hasWeights = true;

// 				if (slot == 4 && acc.componentType ==
// fastgltf::ComponentType::UnsignedShort) {

// 					uint32_t* dst =
// reinterpret_cast<uint32_t*>(beltPtr + offset);

// 					std::visit([&](auto& arg) {
// 						using T =
// std::decay_t<decltype(arg)>; 						if constexpr (requires { arg.bytes.data(); }) {
// 							const std::byte*
// srcBytes = arg.bytes.data() + view.byteOffset + acc.byteOffset;

// 							size_t stride =
// view.byteStride.value_or(sizeof(uint16_t) * 4);

// 							for (size_t k = 0; k <
// acc.count; ++k) { 								const uint16_t* s = reinterpret_cast<const
// uint16_t*>(srcBytes + k * stride);

// 								dst[k*4+0] =
// s[0]; 								dst[k*4+1] = s[1]; 								dst[k*4+2] = s[2]; 								dst[k*4+3] = s[3];
// 							}
// 						}
// 					}, bin.data);

// 					VkDeviceSize copySize = acc.count *
// sizeof(uint32_t) * 4;

// 					vmaFlushAllocation(objs.alloc,
// objs.sbelt.allocation, offset, copySize);

// 					VkBufferCopy region{offset, 0,
// copySize}; 					vkCmdCopyBuffer(cbuffer, beltBuffer,
// mgltfobjs.vbos[i][j][slot].buffer, 1, &region);
// 					add_barrier(mgltfobjs.vbos[i][j][slot].buffer);

// 					offset = (offset + region.size + 15) &
// ~15;
// 				}
// 				else {
// 					vkvbo::record_upload(objs, cbuffer,
// (GpuBuffer&)mgltfobjs.vbos[i][j][slot], 					                     bin, view, acc, beltPtr, beltBuffer,
// offset); 					add_barrier(mgltfobjs.vbos[i][j][slot].buffer);

// 					size_t s = acc.count *
// fastgltf::getElementByteSize(acc.type, acc.componentType); 					offset = (offset +
// s + 15) & ~15;
// 				}
// 			};
// 			std::ranges::for_each(prim.attributes, upload_attr);

// 			if (vertexCount > 0) {
// 				if (!hasJoints) {
// 					size_t size = vertexCount *
// sizeof(uint8_t) * 4; 					std::memset(beltPtr + offset, 0, size);

// 					VkBufferCopy region{offset, 0, size};
// 					vkCmdCopyBuffer(cbuffer, beltBuffer,
// mgltfobjs.vbos[i][j][4].buffer, 1, &region);
// 					add_barrier(mgltfobjs.vbos[i][j][4].buffer);
// 					offset = (offset + size + 15) & ~15;
// 				}
// 				if (!hasWeights) {
// 					size_t size = vertexCount *
// sizeof(float) * 4; 					float* dst = reinterpret_cast<float*>(beltPtr + offset);
// 					for(size_t k=0; k<vertexCount; ++k) {
// 						dst[k*4+0] = 1.0f;
// 						dst[k*4+1] = 0.0f;
// 						dst[k*4+2] = 0.0f;
// 						dst[k*4+3] = 0.0f;
// 					}
// 					VkBufferCopy region{offset, 0, size};
// 					vkCmdCopyBuffer(cbuffer, beltBuffer,
// mgltfobjs.vbos[i][j][5].buffer, 1, &region);
// 					add_barrier(mgltfobjs.vbos[i][j][5].buffer);
// 					offset = (offset + size + 15) & ~15;
// 				}
// 			}
// 		}
// 	}

// 	// might make a namespaced one
// 	if (!barriers.empty()) {
// 		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
// 		dep.bufferMemoryBarrierCount =
// static_cast<uint32_t>(barriers.size()); 		dep.pBufferMemoryBarriers =
// barriers.data(); 		vkCmdPipelineBarrier2(cbuffer, &dep);
// 	}
// }

size_t genericmodel::gettricount(size_t i, size_t j) {
	const fastgltf::Primitive &prim = mmodel2.meshes.at(i).primitives.at(j);

	if (prim.indicesAccessor.has_value()) {
		const fastgltf::Accessor &acc =
		    mmodel2.accessors.at(prim.indicesAccessor.value());
		return acc.count / 3;
	} else {
		if (auto *posAttr = prim.findAttribute("POSITION")) {
			const fastgltf::Accessor &acc =
			    mmodel2.accessors.at(posAttr->accessorIndex);
			return acc.count / 3;
		}

		return 0;
	}
}
void genericmodel::drawinstanced(rvkbucket &objs, VkPipelineLayout &vkplayout,
                                 VkPipeline &vkpline, VkPipeline &vkplineuint,
                                 int instancecount, int stride, uint32_t modelID, uint32_t indirectoffset) {
	VkDeviceSize offset = 0;

//   vkCmdBindDescriptorSets(
//       objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
//       VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1, &mgltfobjs.dset, 0,
//       nullptr);

//   vkCmdBindDescriptorSets(
//       objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
//       VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 4, 1,
//       &rvkbucket::global_materials.dset, 0, nullptr);

	for (size_t i = 0; i < mgltfobjs.vbos.size(); i++) {
		for (size_t j = 0; j < mgltfobjs.vbos.at(i).size(); j++) {
			VkPipeline pipeline = meshjointtype[i][j] ? vkplineuint : vkpline;
			vkCmdBindPipeline(objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			vkpushconstants push{};
			auto &buffers2 = mgltfobjs.vbos[i][j];
			push.stride = stride;
			push.t = static_cast<float>(SDL_GetTicks()) / 1000.0f;
			push.modelID = modelID;

			push.frameIndex = rvkbucket::currentFrame;

			uint32_t local_idx = mmodel2.meshes.at(i).primitives.at(j).materialIndex.has_value()
			                     ? mmodel2.meshes.at(i).primitives.at(j).materialIndex.value()
			                     : mmodel2.materials.size(); // default fallback

			push.materialID = this->m_global_material_indices[local_idx];

			push.posIdx    = buffers2[0].buffer != VK_NULL_HANDLE ? buffers2[0].bindless_idx : 0;
			push.normIdx   = buffers2[1].buffer != VK_NULL_HANDLE ? buffers2[1].bindless_idx : 0;
			push.uvIdx     = buffers2[3].buffer != VK_NULL_HANDLE ? buffers2[3].bindless_idx : 0;
			push.jointIdx  = buffers2[4].buffer != VK_NULL_HANDLE ? buffers2[4].bindless_idx : 0;
			push.weightIdx = buffers2[5].buffer != VK_NULL_HANDLE ? buffers2[5].bindless_idx : 0;

			push.jointFmt = 0;
			push.weightFmt = 2;


			const auto &prim = mmodel2.meshes.at(i).primitives.at(j);
			uint32_t drawCount = static_cast<uint32_t>(gettricount(i, j) * 3);

			if (buffers2[4].buffer != VK_NULL_HANDLE) {
				auto *jAttr = prim.findAttribute("JOINTS_0");

				if (jAttr) {
					auto &jAcc = mmodel2.accessors[jAttr->accessorIndex];
					push.jointFmt = (jAcc.componentType == fastgltf::ComponentType::UnsignedShort) ? 1 : 0;
				}
			}

			if (buffers2[5].buffer != VK_NULL_HANDLE) {
				auto *wAttr = prim.findAttribute("WEIGHTS_0");

				if (wAttr) {
					auto &wAcc = mmodel2.accessors[wAttr->accessorIndex];

					if (wAcc.componentType == fastgltf::ComponentType::Float) push.weightFmt = 2;
					else if (wAcc.componentType == fastgltf::ComponentType::UnsignedShort) push.weightFmt = 1;
					else push.weightFmt = 0;
				}
			}

			vkCmdPushConstants(objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), vkplayout,
			                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0,
			                   sizeof(vkpushconstants), &push);


			if (prim.indicesAccessor.has_value()) {
				const fastgltf::Accessor &idxAcc =
				    mmodel2.accessors.at(prim.indicesAccessor.value());

				VkIndexType vkIndexType = VK_INDEX_TYPE_UINT16;

				switch (idxAcc.componentType) {
					case fastgltf::ComponentType::UnsignedByte:
						vkIndexType = VK_INDEX_TYPE_UINT8_EXT;
						break;

					case fastgltf::ComponentType::UnsignedShort:
						vkIndexType = VK_INDEX_TYPE_UINT16;
						break;

					case fastgltf::ComponentType::UnsignedInt:
						vkIndexType = VK_INDEX_TYPE_UINT32;
						break;

					default:
						break;
				}

				vkCmdBindIndexBuffer(objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
				                     mgltfobjs.ebos.at(i).at(j).buffer, 0, vkIndexType);

				// vkCmdDrawIndexedIndirect(
				//     objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame),
				//     objs.globalIndirectBuffers[rvkbucket::currentFrame].buffer,
				//     indirectoffset * sizeof(VkDrawIndexedIndirectCommand),
				//     instancecount,
				//     sizeof(VkDrawIndexedIndirectCommand)
				// );

				vkCmdDrawIndexed(objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), drawCount,
				                 instancecount, 0, 0, 0);

			} else {
				vkCmdDraw(objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), drawCount, instancecount, 0, 0);
			}
		}
	}
}

void genericmodel::cleanup(rvkbucket &objs) {

	std::lock_guard<std::mutex> lk(rvkbucket::global_buffers.mtx);

	for (size_t i{0}; i < mgltfobjs.vbos.size(); i++) {
		for (size_t j{0}; j < mgltfobjs.vbos.at(i).size(); j++) {
			for (size_t k{0}; k < mgltfobjs.vbos.at(i).at(j).size(); k++) {
				uint32_t idx = mgltfobjs.vbos.at(i).at(j).at(k).bindless_idx;

				if (idx != 0) {
					rvkbucket::global_buffers.free_slots.push(idx);
					mgltfobjs.vbos.at(i).at(j).at(k).bindless_idx = 0; // Nullify
				}

				safe_cleanup(objs, mgltfobjs.vbos.at(i).at(j).at(k));
			}
		}
	}

	for (size_t i{0}; i < mgltfobjs.ebos.size(); i++) {
		for (size_t j{0}; j < mgltfobjs.ebos.at(i).size(); j++) {
			safe_cleanup(objs, mgltfobjs.ebos.at(i).at(j));
		}
	}

	for (size_t i{0}; i < mgltfobjs.texs.size(); i++) {
		rview::rvk::tex::cleanup(objs, mgltfobjs.texs[i]);
	}

	std::lock_guard<std::mutex> lock(rvkbucket::global_materials.mtx);

	for (uint32_t slot : m_global_material_indices) {
		rvkbucket::global_materials.free_slots.push(slot);
	}

	m_global_material_indices.clear();
	// temp
//   rview::rvk::tex::cleanuptpl(objs, *rvkbucket::texlayout, mgltfobjs.texpool);
}
std::vector<texdata> genericmodel::gettexdata() {
	return mgltfobjs.texs;
}
