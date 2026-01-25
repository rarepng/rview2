
#include "core/rvk.hpp"
#include "fastgltf/types.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <iostream>
#include "fastgltf/tools.hpp"


#include "genericmodel.hpp"
#include "vkvbo.hpp"

import rview.rvk.tex;

bool genericmodel::loadmodel(rvkbucket &objs, std::string fname) {

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
	auto result = rview::rvk::tex::load_batch(
	                  objs,
	                  mgltfobjs.texs,
	                  mmodel2,
	                  error_handler
	              );
	rview::rvk::tex::update_descriptor_set(objs,
	                                       mgltfobjs.texs,
	                                       *rvkbucket::texlayout,
	                                       mgltfobjs.texpool,
	                                       mgltfobjs.dset);
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

	if (mmodel2.skins.size()) {
		getjointdata();
		getinvbindmats();

		mjnodecount = mmodel2.nodes.size();

		getanims();
	} else {
		skinned = false;
	}
	return true;
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
	mnodetojoint.reserve(mmodel2.nodes.size());
	mnodetojoint.resize(mmodel2.nodes.size());

	const fastgltf::Skin &skin = mmodel2.skins.at(0);
	for (unsigned int i = 0; i < skin.joints.size(); ++i) {
		mnodetojoint.at(skin.joints.at(i)) = i;
	}
}

void genericmodel::getinvbindmats() {

	const fastgltf::Skin &skin = mmodel2.skins.at(0);
	size_t invBindMatAccessor = skin.inverseBindMatrices.value();

	const fastgltf::Accessor &accessor = mmodel2.accessors.at(invBindMatAccessor);
	const fastgltf::BufferView &bufferView =
	    mmodel2.bufferViews.at(accessor.bufferViewIndex.value());
	const fastgltf::Buffer &buffer = mmodel2.buffers.at(bufferView.bufferIndex);

	minversebindmats.reserve(skin.joints.size());
	minversebindmats.resize(skin.joints.size());

	size_t newbytelength{skin.joints.size() * 4 * 4 * 4};
	std::visit(fastgltf::visitor{[](auto &arg) {},
	[&](const fastgltf::sources::Array &vector) {
		std::memcpy(minversebindmats.data(),
		            vector.bytes.data() +
		            bufferView.byteOffset +
		            accessor.byteOffset,
		            newbytelength);
	}},
	buffer.data);
}

void genericmodel::getanims() {
	manimclips.reserve(mmodel2.animations.size());
	for (auto &anim0 : mmodel2.animations) {
		std::shared_ptr<vkclip> clip0 =
		    std::make_shared<vkclip>(static_cast<std::string>(anim0.name));
		for (auto &c : anim0.channels) {
			clip0->addchan(mmodel2, anim0, c);
		}
		manimclips.push_back(clip0);
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
		[](auto &arg) {},
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
genericmodel::getnodelist(std::vector<std::shared_ptr<vknode>> &nodeList,
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
		{"POSITION", 0}, {"NORMAL", 1}, {"TANGENT", 2},
		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
	};

	for (size_t i = 0; i < meshCount; ++i) {
		size_t primCount = mmodel2.meshes[i].primitives.size();

		mgltfobjs.vbos[i].resize(primCount);
		mgltfobjs.ebos[i].resize(primCount);
		meshjointtype[i].resize(primCount, false);

		for (size_t j = 0; j < primCount; ++j) {
			const auto& prim = mmodel2.meshes[i].primitives[j];
			if (prim.type != fastgltf::PrimitiveType::Triangles) continue;

			if (prim.indicesAccessor.has_value()) {
				const auto& idxAcc = mmodel2.accessors[prim.indicesAccessor.value()];
				size_t indexSize = idxAcc.count * fastgltf::getElementByteSize(idxAcc.type, idxAcc.componentType);

				vkvbo::init(objs, (GpuBuffer&)mgltfobjs.ebos[i][j], indexSize,
				            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			}

			mgltfobjs.vbos[i][j].resize(6);

			bool hasJoints = false;
			bool hasWeights = false;
			size_t vertexCount = 0;

			const auto* posAttr = prim.findAttribute("POSITION");
			if (posAttr) vertexCount = mmodel2.accessors[posAttr->accessorIndex].count;

			auto init_attr = [&](const auto& attr) {
				auto it = SLOT_MAP.find(attr.name);
				if (it == SLOT_MAP.end()) return;
				int slot = it->second;

				const auto& acc = mmodel2.accessors[attr.accessorIndex];
				size_t size = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);

				if (slot == 4) {
					hasJoints = true;
					if (acc.componentType == fastgltf::ComponentType::UnsignedShort || acc.componentType == fastgltf::ComponentType::UnsignedInt) {
						meshjointtype[i][j] = true;
						size = acc.count * sizeof(uint32_t) * 4;
					}
				}

				if (slot == 5) hasWeights = true;

				vkvbo::init(objs, (GpuBuffer&)mgltfobjs.vbos[i][j][slot], size,
				            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			};

			std::ranges::for_each(prim.attributes, init_attr);

			if (vertexCount > 0) {
				if (!hasJoints) {
					size_t size = vertexCount * sizeof(uint8_t) * 4;
					vkvbo::init(objs, (GpuBuffer&)mgltfobjs.vbos[i][j][4], size,
					            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
				}

				if (!hasWeights) {
					size_t size = vertexCount * sizeof(float) * 4;
					vkvbo::init(objs, (GpuBuffer&)mgltfobjs.vbos[i][j][5], size,
					            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
				}
			}
		}
	}
}
const std::byte* getRawData(const fastgltf::DataSource& source) {
	return std::visit([](auto& arg) -> const std::byte* {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (requires { arg.bytes.data(); }) {
			return arg.bytes.data();
		}
		return nullptr;
	}, source);
}
void genericmodel::uploadvboebo(rvkbucket &objs, VkCommandBuffer &cbuffer) {
	static const std::unordered_map<std::string_view, int> SLOT_MAP = {
		{"POSITION", 0}, {"NORMAL", 1}, {"TANGENT", 2},
		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
	};
	uint8_t* beltPtr = objs.sbelt.mappedData;
	VkBuffer beltBuffer = objs.sbelt.buffer;
	VkDeviceSize& offset = objs.sbelt.currentOffset;

	std::vector<VkBufferMemoryBarrier2> barriers;

	// maybe will move this to rvk or a new namespace
	auto add_barrier = [&](VkBuffer buffer) {
		VkBufferMemoryBarrier2 b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
		b.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT;
		b.buffer = buffer;
		b.size = VK_WHOLE_SIZE;
		barriers.push_back(b);
	};

	for (size_t i = 0; i < mmodel2.meshes.size(); ++i) {
		for (size_t j = 0; j < mmodel2.meshes[i].primitives.size(); ++j) {
			const auto& prim = mmodel2.meshes[i].primitives[j];
			if (prim.type != fastgltf::PrimitiveType::Triangles) continue;

			if (prim.indicesAccessor.has_value()) {
				const auto& acc = mmodel2.accessors[prim.indicesAccessor.value()];
				const auto& view = mmodel2.bufferViews[acc.bufferViewIndex.value()];
				const auto& bin = mmodel2.buffers[view.bufferIndex];

				vkvbo::record_upload(objs, cbuffer, (GpuBuffer&)mgltfobjs.ebos[i][j],
				                     bin, view, acc, beltPtr, beltBuffer, offset);
				add_barrier(mgltfobjs.ebos[i][j].buffer);

				size_t s = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);
				offset = (offset + s + 15) & ~15;
			}

			bool hasJoints = false;
			bool hasWeights = false;
			size_t vertexCount = 0;
			if (auto* p = prim.findAttribute("POSITION")) vertexCount = mmodel2.accessors[p->accessorIndex].count;

			auto upload_attr = [&](const auto& attr) {
				auto it = SLOT_MAP.find(attr.name);
				if (it == SLOT_MAP.end()) return;
				int slot = it->second;

				const auto& acc = mmodel2.accessors[attr.accessorIndex];
				const auto& view = mmodel2.bufferViews[acc.bufferViewIndex.value()];
				const auto& bin = mmodel2.buffers[view.bufferIndex];

				if (slot == 4) hasJoints = true;
				if (slot == 5) hasWeights = true;

if (slot == 4 && acc.componentType == fastgltf::ComponentType::UnsignedShort) {
                    
                    uint32_t* dst = reinterpret_cast<uint32_t*>(beltPtr + offset);

                    std::visit([&](auto& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (requires { arg.bytes.data(); }) {
                            const std::byte* srcBytes = arg.bytes.data() + view.byteOffset + acc.byteOffset;
                            
                            size_t stride = view.byteStride.value_or(sizeof(uint16_t) * 4);

                            for (size_t k = 0; k < acc.count; ++k) {
                                const uint16_t* s = reinterpret_cast<const uint16_t*>(srcBytes + k * stride);
                                
                                dst[k*4+0] = s[0]; dst[k*4+1] = s[1];
                                dst[k*4+2] = s[2]; dst[k*4+3] = s[3];
                            }
                        }
                    }, bin.data);

                    VkDeviceSize copySize = acc.count * sizeof(uint32_t) * 4;
                    
                    vmaFlushAllocation(objs.alloc, objs.sbelt.allocation, offset, copySize);

                    VkBufferCopy region{offset, 0, copySize};
                    vkCmdCopyBuffer(cbuffer, beltBuffer, mgltfobjs.vbos[i][j][slot].buffer, 1, &region);
                    add_barrier(mgltfobjs.vbos[i][j][slot].buffer);

                    offset = (offset + region.size + 15) & ~15;
                }
				else {
					vkvbo::record_upload(objs, cbuffer, (GpuBuffer&)mgltfobjs.vbos[i][j][slot],
					                     bin, view, acc, beltPtr, beltBuffer, offset);
					add_barrier(mgltfobjs.vbos[i][j][slot].buffer);

					size_t s = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);
					offset = (offset + s + 15) & ~15;
				}
			};
			std::ranges::for_each(prim.attributes, upload_attr);

			if (vertexCount > 0) {
				if (!hasJoints) {
					size_t size = vertexCount * sizeof(uint8_t) * 4;
					std::memset(beltPtr + offset, 0, size);

					VkBufferCopy region{offset, 0, size};
					vkCmdCopyBuffer(cbuffer, beltBuffer, mgltfobjs.vbos[i][j][4].buffer, 1, &region);
					add_barrier(mgltfobjs.vbos[i][j][4].buffer);
					offset = (offset + size + 15) & ~15;
				}
				if (!hasWeights) {
					size_t size = vertexCount * sizeof(float) * 4;
					float* dst = reinterpret_cast<float*>(beltPtr + offset);
					for(size_t k=0; k<vertexCount; ++k) {
						dst[k*4+0] = 1.0f;
						dst[k*4+1] = 0.0f;
						dst[k*4+2] = 0.0f;
						dst[k*4+3] = 0.0f;
					}
					VkBufferCopy region{offset, 0, size};
					vkCmdCopyBuffer(cbuffer, beltBuffer, mgltfobjs.vbos[i][j][5].buffer, 1, &region);
					add_barrier(mgltfobjs.vbos[i][j][5].buffer);
					offset = (offset + size + 15) & ~15;
				}
			}
		}
	}

	// might make a namespaced one
	if (!barriers.empty()) {
		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size());
		dep.pBufferMemoryBarriers = barriers.data();
		vkCmdPipelineBarrier2(cbuffer, &dep);
	}
}

size_t genericmodel::gettricount(size_t i, size_t j) {
	const fastgltf::Primitive &prims = mmodel2.meshes.at(i).primitives.at(j);
	const fastgltf::Accessor &acc =
	    mmodel2.accessors.at(prims.indicesAccessor.value());
	size_t c{acc.count / 3};
	return c;
}

void genericmodel::drawinstanced(rvkbucket &objs, VkPipelineLayout &vkplayout,
                                 VkPipeline &vkpline, VkPipeline &vkplineuint,
                                 int instancecount, int stride) {
	VkDeviceSize offset = 0;

	vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(rvkbucket::currentFrame),
	                        VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1,
	                        &mgltfobjs.dset, 0, nullptr);
	for (size_t i = 0; i < mgltfobjs.vbos.size(); i++) {
		for (size_t j = 0; j < mgltfobjs.vbos.at(i).size(); j++) {
			VkPipeline pipeline = meshjointtype[i][j] ? vkplineuint : vkpline;
			vkCmdBindPipeline(objs.cbuffers_graphics.at(rvkbucket::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			vkpushconstants push{};
			auto& buffers2 = mgltfobjs.vbos[i][j];
			bool hasSkin = (buffers2[4].buffer != VK_NULL_HANDLE && buffers2[5].buffer != VK_NULL_HANDLE);
			push.stride = hasSkin ? stride : 0;
			push.envMapMaxLod = rvkbucket::hdrmiplod - 1;
			push.stride = stride;
			push.t = static_cast<float>(SDL_GetTicks()) / 1000.0f;

			const fastgltf::Material* mat = nullptr;
			if (mmodel2.meshes.at(i).primitives.at(j).materialIndex.has_value()) {
				mat = &mmodel2.materials[mmodel2.meshes.at(i).primitives.at(j).materialIndex.value()];
			}

			if (mat) {
				push.baseColorFactor = glm::make_vec4(mat->pbrData.baseColorFactor.data());
				push.roughnessFactor = mat->pbrData.roughnessFactor;
				push.metallicFactor  = mat->pbrData.metallicFactor;
				push.emissiveFactor  = glm::make_vec3(mat->emissiveFactor.data());
				push.normalScale     = 1.0f;
				if (mat->normalTexture.has_value()) push.normalScale = mat->normalTexture.value().scale;
				if (mat->pbrData.baseColorTexture.has_value()) {
					size_t texIdx = mat->pbrData.baseColorTexture.value().textureIndex;
					size_t imgIdx = mmodel2.textures[texIdx].imageIndex.value();
					push.albedoIdx = static_cast<uint32_t>(imgIdx + 4);
				} else {
					push.albedoIdx = 1;
				}

				if (mat->normalTexture.has_value()) {
					size_t texIdx = mat->normalTexture.value().textureIndex;
					size_t imgIdx = mmodel2.textures[texIdx].imageIndex.value();
					push.normalIdx = static_cast<uint32_t>(imgIdx + 4);
				} else {
					push.normalIdx = 2;
				}

				if (mat->pbrData.metallicRoughnessTexture.has_value()) {
					size_t texIdx = mat->pbrData.metallicRoughnessTexture.value().textureIndex;
					size_t imgIdx = mmodel2.textures[texIdx].imageIndex.value();
					push.ormIdx = static_cast<uint32_t>(imgIdx + 4);
				} else {
					push.ormIdx = 1;
				}
				if (mat->emissiveTexture.has_value()) {
					size_t texIdx = mat->emissiveTexture.value().textureIndex;
					size_t imgIdx = mmodel2.textures[texIdx].imageIndex.value();
					push.emissiveIdx = static_cast<uint32_t>(imgIdx + 4);
				} else {
					push.emissiveIdx = 3;
				}
				push.transmissionIdx = 3;
				push.sheenIdx = 3;
				push.clearcoatIdx = 3;
				push.thicknessIdx = 1;

			} else {
				push.baseColorFactor = glm::vec4(1.0f);
				push.roughnessFactor = 1.0f;
				push.metallicFactor = 0.0f;
				push.albedoIdx = 1;
				push.normalIdx = 2;
				push.ormIdx = 1;
				push.emissiveIdx = 3;
			}
			vkCmdPushConstants(objs.cbuffers_graphics.at(rvkbucket::currentFrame),
			                   vkplayout,
			                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			                   0,
			                   sizeof(vkpushconstants), &push);

			auto& buffers = mgltfobjs.vbos.at(i).at(j);
			VkBuffer dummy = buffers[0].buffer;//fml help idk
			for (uint32_t binding = 0; binding < 6; binding++) {
				VkBuffer bufToBind = dummy;

				if (binding < buffers.size() && buffers[binding].buffer != VK_NULL_HANDLE) {
					bufToBind = buffers[binding].buffer;
				}

				vkCmdBindVertexBuffers(objs.cbuffers_graphics.at(rvkbucket::currentFrame),
				                       binding, 1, &bufToBind, &offset);
			}

			vkCmdBindIndexBuffer(objs.cbuffers_graphics.at(rvkbucket::currentFrame),
			                     mgltfobjs.ebos.at(i).at(j).buffer, 0,
			                     VK_INDEX_TYPE_UINT16);

			vkCmdDrawIndexed(objs.cbuffers_graphics.at(rvkbucket::currentFrame),
			                 static_cast<uint32_t>(gettricount(i, j) * 3),
			                 instancecount, 0, 0, 0);
		}
	}
}

void genericmodel::cleanup(rvkbucket &objs) {

	for (size_t i{0}; i < mgltfobjs.vbos.size(); i++) {
		for (size_t j{0}; j < mgltfobjs.vbos.at(i).size(); j++) {
			for (size_t k{0}; k < mgltfobjs.vbos.at(i).at(j).size(); k++) {
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
	//temp
	rview::rvk::tex::cleanuptpl(objs,*rvkbucket::texlayout,mgltfobjs.texpool);
}
std::vector<texdata> genericmodel::gettexdata() {
	return mgltfobjs.texs;
}
