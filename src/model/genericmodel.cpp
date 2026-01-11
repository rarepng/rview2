
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

#include "vktexture.hpp"

#include "genericmodel.hpp"
#include "vkebo.hpp"
#include "vkvbo.hpp"

bool genericmodel::loadmodel(rvk &objs, std::string fname) {

	fastgltf::Parser fastparser{};
	auto buff = fastgltf::MappedGltfFile::FromPath(fname);
	auto a =
	    fastparser.loadGltfBinary(buff.get(), std::filesystem::absolute(fname));
	mmodel2 = std::move(a.get());

	mgltfobjs.texs.reserve(mmodel2.images.size());
	mgltfobjs.texs.resize(mmodel2.images.size());
	static const bool _ = [&] {
		if (!vktexture::createlayout(objs))
			return false;
		return true;
	}();

	if (!vktexture::loadtexture(objs, mgltfobjs.texs, mmodel2))
		return false;
	if (!vktexture::loadtexset(objs, mgltfobjs.texs, *rvk::texlayout,
	                           mgltfobjs.dset, mmodel2))
		return false;

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

void genericmodel::createvboebo(rvk &objs) {
	mgltfobjs.vbos.resize(mmodel2.meshes.size());
	mgltfobjs.ebos.resize(mmodel2.meshes.size());

	meshjointtype.resize(mmodel2.meshes.size());

	static const std::unordered_map<std::string_view, int> SLOT_MAP = {
		{"POSITION", 0}, {"NORMAL", 1}, {"TANGENT", 2},
		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
	};

	for (size_t i = 0; i < mmodel2.meshes.size(); ++i) {
		size_t primCount = mmodel2.meshes[i].primitives.size();
		mgltfobjs.vbos[i].resize(primCount);
		mgltfobjs.ebos[i].resize(primCount);

		meshjointtype[i].resize(primCount, false);

		for (size_t j = 0; j < primCount; ++j) {
			const auto& prim = mmodel2.meshes[i].primitives[j];
			if (prim.type != fastgltf::PrimitiveType::Triangles) continue;

			if (prim.indicesAccessor.has_value()) {
				const auto& idxAcc = mmodel2.accessors[prim.indicesAccessor.value()];
				size_t indexSize = idxAcc.count * fastgltf::getComponentByteSize(idxAcc.componentType);

				vkebo::init(objs, mgltfobjs.ebos[i][j], indexSize);
			}

			mgltfobjs.vbos[i][j].resize(6);
			mgltfobjs.vbos[i][j][4].buffer = VK_NULL_HANDLE;
			mgltfobjs.vbos[i][j][5].buffer = VK_NULL_HANDLE;

			auto init_attr = [&](const auto& attr) {
				auto it = SLOT_MAP.find(attr.name);
				if (it == SLOT_MAP.end()) return;
				int slot = it->second;

				const auto& acc = mmodel2.accessors[attr.accessorIndex];
				size_t size = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);

				if (slot == 4) {
					if (acc.componentType == fastgltf::ComponentType::UnsignedInt ||
					        acc.componentType == fastgltf::ComponentType::UnsignedShort) {

						size = acc.count * sizeof(uint32_t) * 4;

						meshjointtype[i][j] = true;
					}
				}
				vkvbo::init(objs, mgltfobjs.vbos[i][j][slot], size);
			};
			std::ranges::for_each(prim.attributes, init_attr);
		}
	}
}
void genericmodel::uploadvboebo(rvk &objs, VkCommandBuffer &cbuffer) {
	static const std::unordered_map<std::string_view, int> SLOT_MAP = {
		{"POSITION", 0}, {"NORMAL", 1}, {"TANGENT", 2},
		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
	};

	for (size_t i = 0; i < mmodel2.meshes.size(); ++i) {
		for (size_t j = 0; j < mmodel2.meshes[i].primitives.size(); ++j) {
			const auto& prim = mmodel2.meshes[i].primitives[j];
			if (prim.type != fastgltf::PrimitiveType::Triangles) continue;

			const auto& idxAcc = mmodel2.accessors[prim.indicesAccessor.value()];
			const auto& idxView = mmodel2.bufferViews[idxAcc.bufferViewIndex.value()];

			const auto& idxBuffer = mmodel2.buffers[idxView.bufferIndex];
			vkebo::upload(objs, cbuffer, mgltfobjs.ebos[i][j],
			              idxBuffer,
			              idxView,
			              idxAcc.count,
			              idxAcc.componentType);
			auto upload_attr = [&](const auto& attr) {
				auto it = SLOT_MAP.find(attr.name);
				if (it == SLOT_MAP.end()) return;
				int slot = it->second;

				const auto& acc = mmodel2.accessors[attr.accessorIndex];
				const auto& view = mmodel2.bufferViews[acc.bufferViewIndex.value()];
				const auto& bin = mmodel2.buffers[view.bufferIndex];

				// todo: retake a look here
				if (slot == 4 && acc.componentType == fastgltf::ComponentType::UnsignedShort) {
					std::vector<uint32_t> ints(acc.count * 4);

					std::visit(fastgltf::visitor{
						[&](auto& arg) {
							using T = std::decay_t<decltype(arg)>;
							if constexpr (requires { arg.bytes.data(); }) {
								const uint8_t* pData = reinterpret_cast<const uint8_t*>(arg.bytes.data());

								const uint8_t* pSource = pData + view.byteOffset + acc.byteOffset;

								size_t stride = view.byteStride.has_value() ? view.byteStride.value() : (sizeof(uint16_t) * 4);

								for (size_t i = 0; i < acc.count; ++i) {
									const uint16_t* pShorts = reinterpret_cast<const uint16_t*>(pSource + (i * stride));

									ints[i * 4 + 0] = static_cast<uint32_t>(pShorts[0]);
									ints[i * 4 + 1] = static_cast<uint32_t>(pShorts[1]);
									ints[i * 4 + 2] = static_cast<uint32_t>(pShorts[2]);
									ints[i * 4 + 3] = static_cast<uint32_t>(pShorts[3]);
								}
							}
						}
					}, bin.data);
					vkvbo::upload(objs, cbuffer, mgltfobjs.vbos[i][j][slot], ints);
				}
				else {
					vkvbo::upload(objs, cbuffer, mgltfobjs.vbos[i][j][slot], bin, view, acc);
				}
			};
			std::ranges::for_each(prim.attributes, upload_attr);
		}
	}
}
size_t genericmodel::gettricount(size_t i, size_t j) {
	const fastgltf::Primitive &prims = mmodel2.meshes.at(i).primitives.at(j);
	const fastgltf::Accessor &acc =
	    mmodel2.accessors.at(prims.indicesAccessor.value());
	size_t c{acc.count / 3};
	return c;
}

void genericmodel::drawinstanced(rvk &objs, VkPipelineLayout &vkplayout,
                                 VkPipeline &vkpline, VkPipeline &vkplineuint,
                                 int instancecount, int stride) {
	VkDeviceSize offset = 0;

	vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(rvk::currentFrame),
	                        VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1,
	                        &mgltfobjs.dset, 0, nullptr);
	for (size_t i = 0; i < mgltfobjs.vbos.size(); i++) {
		for (size_t j = 0; j < mgltfobjs.vbos.at(i).size(); j++) {
			VkPipeline pipeline = meshjointtype[i][j] ? vkplineuint : vkpline;
			vkCmdBindPipeline(objs.cbuffers_graphics.at(rvk::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			vkpushconstants push{};
			auto& buffers2 = mgltfobjs.vbos[i][j];
			bool hasSkin = (buffers2[4].buffer != VK_NULL_HANDLE && buffers2[5].buffer != VK_NULL_HANDLE);
			push.stride = hasSkin ? stride : 0;

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
			vkCmdPushConstants(objs.cbuffers_graphics.at(rvk::currentFrame),
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

				vkCmdBindVertexBuffers(objs.cbuffers_graphics.at(rvk::currentFrame),
				                       binding, 1, &bufToBind, &offset);
			}

			vkCmdBindIndexBuffer(objs.cbuffers_graphics.at(rvk::currentFrame),
			                     mgltfobjs.ebos.at(i).at(j).buffer, 0,
			                     VK_INDEX_TYPE_UINT16);

			vkCmdDrawIndexed(objs.cbuffers_graphics.at(rvk::currentFrame),
			                 static_cast<uint32_t>(gettricount(i, j) * 3),
			                 instancecount, 0, 0, 0);
		}
	}
}

void genericmodel::cleanup(rvk &objs) {

	for (size_t i{0}; i < mgltfobjs.vbos.size(); i++) {
		for (size_t j{0}; j < mgltfobjs.vbos.at(i).size(); j++) {
			for (size_t k{0}; k < mgltfobjs.vbos.at(i).at(j).size(); k++) {
				vkvbo::cleanup(objs, mgltfobjs.vbos.at(i).at(j).at(k));
			}
		}
	}
	for (size_t i{0}; i < mgltfobjs.ebos.size(); i++) {
		for (size_t j{0}; j < mgltfobjs.ebos.at(i).size(); j++) {
			vkebo::cleanup(objs, mgltfobjs.ebos.at(i).at(j));
		}
	}
	for (size_t i{0}; i < mgltfobjs.texs.size(); i++) {
		vktexture::cleanup(objs, mgltfobjs.texs[i]);
	}
}
std::vector<texdata> genericmodel::gettexdata() {
	return mgltfobjs.texs;
}
