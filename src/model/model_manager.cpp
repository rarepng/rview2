#include <model_manager.hpp>
#include <vkvbo.hpp>
#include <anim/vkclip.hpp>
#include <anim/vknode.hpp>
#include <fstream>
#include <simdjson.h>
#include <unordered_map>
#include <iostream>
#include <stb_image.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

namespace fastgltf {
template <> struct ElementTraits<std::array<int8_t, 3>> : ElementTraitsBase<std::array<int8_t, 3>, AccessorType::Vec3, int8_t> {};
template <> struct ElementTraits<std::array<int16_t, 3>> : ElementTraitsBase<std::array<int16_t, 3>, AccessorType::Vec3, int16_t> {};
template <> struct ElementTraits<std::array<float, 3>> : ElementTraitsBase<std::array<float, 3>, AccessorType::Vec3, float> {};
}

namespace model_manager {

static std::unordered_map<uint32_t, GpuBuffer> g_modelVBOs;

std::string get_glb_json_chunk(std::string_view filepath) {
	std::ifstream file(filepath.data(), std::ios::binary);

	if (!file) return "";

	uint32_t magic, version, length;
	file.read(reinterpret_cast<char*>(&magic), 4);

	if (magic != 0x46546C67) return ""; // magic gltf

	file.read(reinterpret_cast<char*>(&version), 4);
	file.read(reinterpret_cast<char*>(&length), 4);

	uint32_t chunkLength, chunkType;
	file.read(reinterpret_cast<char*>(&chunkLength), 4);
	file.read(reinterpret_cast<char*>(&chunkType), 4);

	if (chunkType != 0x4E4F534A) return ""; // magic json

	std::string json(chunkLength, ' ');
	file.read(json.data(), chunkLength);
	return json;
}

StagingModelData parse_model_to_staging(std::string_view filepath) {
	StagingModelData staging;
	staging.name = filepath;

	fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
	auto mappedFile = fastgltf::MappedGltfFile::FromPath(filepath);

	if (!mappedFile) return staging;

	auto assetResult = parser.loadGltfBinary(mappedFile.get(), std::filesystem::absolute(filepath));

	if (assetResult.error() != fastgltf::Error::None) {
		std::cerr << "fastgltf error loading: " << filepath << "\n";
		return staging;
	}

	fastgltf::Asset& asset = assetResult.get();

	staging.textures.reserve(asset.images.size());

	for (size_t i = 0; i < asset.images.size(); ++i) {
		auto& img_asset = asset.images[i];
		StagingTexture st{};
		st.imageIndex = i;
		st.name = std::string(img_asset.name);

		int w, h, c;
		stbi_uc* pixels = nullptr;

		std::visit(fastgltf::visitor{
			[](auto&) {},
			[&](fastgltf::sources::BufferView & view) {
				auto& bv = asset.bufferViews[view.bufferViewIndex];
				auto& buf = asset.buffers[bv.bufferIndex];
				std::visit(fastgltf::visitor{
					[](auto&) {},
					[&](fastgltf::sources::Array & vec) {
						auto* data_ptr = reinterpret_cast<const stbi_uc*>(vec.bytes.data() + bv.byteOffset);
						pixels = stbi_load_from_memory(data_ptr, static_cast<int>(bv.byteLength), &w, &h, &c, 4);
					}
				}, buf.data);
			},
			[&](fastgltf::sources::Array & vec) {
				auto* data_ptr = reinterpret_cast<const stbi_uc*>(vec.bytes.data());
				pixels = stbi_load_from_memory(data_ptr, static_cast<int>(vec.bytes.size()), &w, &h, &c, 4);
			}
		}, img_asset.data);

		if (pixels) {
			st.width = w;
			st.height = h;
			size_t byteSize = w * h * 4;
			st.pixels.resize(byteSize);
			std::memcpy(st.pixels.data(), pixels, byteSize);
			stbi_image_free(pixels);
		} else {
			std::cerr << "stbi_load failed to decode: " << st.name << "\n";
		}

		staging.textures.push_back(std::move(st));
	}

	staging.materials.reserve(asset.materials.size());

	for (const auto& mat : asset.materials) {
		StagingMaterial sm{};
		sm.data.envMapMaxLod = rview::core::hdrmiplod - 1;
		sm.data.baseColorFactor = glm::make_vec4(mat.pbrData.baseColorFactor.data());
		sm.data.roughnessFactor = mat.pbrData.roughnessFactor;
		sm.data.metallicFactor = mat.pbrData.metallicFactor;
		sm.data.emissiveFactor = glm::make_vec3(mat.emissiveFactor.data());
		sm.data.normalScale = mat.normalTexture.has_value() ? mat.normalTexture.value().scale : 1.0f;

		sm.data.albedoIdx       = 1;
		sm.data.normalIdx       = 2;
		sm.data.ormIdx          = 1;
		sm.data.emissiveIdx     = 3;
		sm.data.transmissionIdx = 3;
		sm.data.sheenIdx        = 3;
		sm.data.clearcoatIdx    = 3;
		sm.data.thicknessIdx    = 1;

		if (mat.pbrData.baseColorTexture.has_value()) {
			sm.localAlbedoImgIdx = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
		}

		if (mat.normalTexture.has_value()) {
			sm.localNormalImgIdx = asset.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
		}

		if (mat.pbrData.metallicRoughnessTexture.has_value()) {
			sm.localOrmImgIdx = asset.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
		}

		if (mat.emissiveTexture.has_value()) {
			sm.localEmissiveImgIdx = asset.textures[mat.emissiveTexture.value().textureIndex].imageIndex.value();
		}

		staging.materials.push_back(sm);
	}

	std::string jsonChunk = get_glb_json_chunk(filepath);

	if (!jsonChunk.empty()) {
		simdjson::ondemand::parser jsonParser;
		simdjson::padded_string paddedJson(jsonChunk);
		simdjson::ondemand::document doc;

		if (!jsonParser.iterate(paddedJson).get(doc)) {
			simdjson::ondemand::array jsonMats;

			if (!doc["materials"].get_array().get(jsonMats)) {
				size_t matIdx = 0;

				for (auto matObj : jsonMats) {
					if (matIdx >= staging.materials.size()) break;

					simdjson::ondemand::object extensions;

					if (!matObj["extensions"].get_object().get(extensions)) {
						simdjson::ondemand::object mtoon;

						if (!extensions["VRMC_materials_mtoon"].get_object().get(mtoon)) {
							StagingMaterial& sm = staging.materials[matIdx];
							sm.isMToon = true;

							simdjson::ondemand::array shadeArr;

							if (!mtoon["shadeColorFactor"].get_array().get(shadeArr)) {
								int i = 0;

								for (double val : shadeArr) {
									if (i == 0) sm.mtoonShadeColorFactor.r = static_cast<float>(val);

									if (i == 1) sm.mtoonShadeColorFactor.g = static_cast<float>(val);

									if (i == 2) sm.mtoonShadeColorFactor.b = static_cast<float>(val);

									i++;
								}
							}

							simdjson::ondemand::object shadeTex;

							if (!mtoon["shadeMultiplyTexture"].get_object().get(shadeTex)) {
								uint64_t texIdx;

								if (!shadeTex["index"].get_uint64().get(texIdx)) {
									sm.localShadeImgIdx = asset.textures[texIdx].imageIndex.value();
								}
							}

							double shift, toony;

							if (!mtoon["shadingShiftFactor"].get_double().get(shift)) {
								sm.mtoonShadingShiftFactor = static_cast<float>(shift);
							}

							if (!mtoon["shadingToonyFactor"].get_double().get(toony)) {
								sm.mtoonShadingToonyFactor = static_cast<float>(toony);
							}
						}
					}

					matIdx++;
				}
			}
		}
	}

	static const std::unordered_map<std::string_view, int> SLOT_MAP = {
		{"POSITION", 0}, {"NORMAL", 1}, {"TANGENT", 2},
		{"TEXCOORD_0", 3}, {"JOINTS_0", 4}, {"WEIGHTS_0", 5}
	};

	staging.meshes.resize(asset.meshes.size());
	staging.strictAlignedTotalBytes = 0;

	for (size_t i = 0; i < asset.meshes.size(); ++i) {
		const auto& inMesh = asset.meshes[i];

		for (size_t j = 0; j < inMesh.primitives.size(); ++j) {
			const auto& inPrim = inMesh.primitives[j];

			if (inPrim.type != fastgltf::PrimitiveType::Triangles) continue;

			StagingPrimitive outPrim{};
			outPrim.meta = PrimitiveMetadata{};
			outPrim.meta.materialID = inPrim.materialIndex.has_value() ? inPrim.materialIndex.value() : 0xFFFFFFFF;
			outPrim.meta.jointFmt = 0;
			outPrim.meta.weightFmt = 2;

			if (inPrim.indicesAccessor.has_value()) {
				const auto& acc = asset.accessors[inPrim.indicesAccessor.value()];
				outPrim.meta.indexCount = static_cast<uint32_t>(acc.count);

				size_t byteSize = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);
				outPrim.indexBytes.resize(byteSize);

				if (acc.componentType == fastgltf::ComponentType::UnsignedByte) {
					outPrim.meta.indexType = 0;
					fastgltf::copyFromAccessor<uint8_t>(asset, acc, outPrim.indexBytes.data());
				} else if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
					outPrim.meta.indexType = 1;
					fastgltf::copyFromAccessor<uint16_t>(asset, acc, reinterpret_cast<uint16_t*>(outPrim.indexBytes.data()));
				} else {
					outPrim.meta.indexType = 2;
					fastgltf::copyFromAccessor<uint32_t>(asset, acc, reinterpret_cast<uint32_t*>(outPrim.indexBytes.data()));
				}
			} else {
				outPrim.meta.indexType = 3;

				if (auto* posAttr = inPrim.findAttribute("POSITION")) {
					outPrim.meta.indexCount = static_cast<uint32_t>(asset.accessors[posAttr->accessorIndex].count);
				}
			}

			for (const auto& attr : inPrim.attributes) {
				auto it = SLOT_MAP.find(attr.name);

				if (it == SLOT_MAP.end()) continue;

				int slot = it->second;

				const auto& acc = asset.accessors[attr.accessorIndex];
				size_t byteSize = acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType);
				outPrim.vertexBytes[slot].resize(byteSize);

				staging.strictAlignedTotalBytes += ((byteSize + 255) & ~255);

				if (slot == 0 || slot == 1) {
					fastgltf::copyFromAccessor<glm::vec3>(asset, acc, outPrim.vertexBytes[slot].data());

					if (slot == 0 && acc.count > 0) {
						glm::vec3 pmin(std::numeric_limits<float>::max());
						glm::vec3 pmax(std::numeric_limits<float>::lowest());
						glm::vec3* posArray = reinterpret_cast<glm::vec3*>(outPrim.vertexBytes[slot].data());

						for (size_t v = 0; v < acc.count; ++v) {
							pmin = glm::min(pmin, posArray[v]);
							pmax = glm::max(pmax, posArray[v]);
						}

						outPrim.meta.setAABB(pmin, pmax);
					}
				} else if (slot == 2) {
					fastgltf::copyFromAccessor<glm::vec4>(asset, acc, outPrim.vertexBytes[slot].data());
				} else if (slot == 3) {
					fastgltf::copyFromAccessor<glm::vec2>(asset, acc, outPrim.vertexBytes[slot].data());
				} else if (slot == 4) {
					outPrim.meta.jointFmt = (acc.componentType == fastgltf::ComponentType::UnsignedShort) ? 1 : 0;

					if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
						fastgltf::copyFromAccessor<glm::u16vec4>(asset, acc, outPrim.vertexBytes[slot].data());
					} else if (acc.componentType == fastgltf::ComponentType::UnsignedByte) {
						fastgltf::copyFromAccessor<glm::u8vec4>(asset, acc, outPrim.vertexBytes[slot].data());
					} else {
						fastgltf::copyFromAccessor<glm::u32vec4>(asset, acc, outPrim.vertexBytes[slot].data());
					}
				} else if (slot == 5) {
					if (acc.componentType == fastgltf::ComponentType::Float) outPrim.meta.weightFmt = 2;
					else if (acc.componentType == fastgltf::ComponentType::UnsignedShort) outPrim.meta.weightFmt = 1;
					else outPrim.meta.weightFmt = 0;

					if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
						fastgltf::copyFromAccessor<glm::u16vec4>(asset, acc, outPrim.vertexBytes[slot].data());
					} else if (acc.componentType == fastgltf::ComponentType::UnsignedByte) {
						fastgltf::copyFromAccessor<glm::u8vec4>(asset, acc, outPrim.vertexBytes[slot].data());
					} else {
						fastgltf::copyFromAccessor<glm::vec4>(asset, acc, outPrim.vertexBytes[slot].data());
					}
				}
			}

			outPrim.meta.targetCount = static_cast<uint32_t>(inPrim.targets.size());
			outPrim.meta.morphDeltaIdx = 0xFFFFFFFF;
			outPrim.meta.morphFormat = 0;

			if (outPrim.meta.targetCount > 0) {
				size_t vertexCount = 0;

				if (auto* posAttr = inPrim.findAttribute("POSITION")) {
					vertexCount = asset.accessors[posAttr->accessorIndex].count;
					outPrim.meta.vertexCount = static_cast<uint32_t>(vertexCount);
				}

				if (vertexCount > 0) {
					auto findTargetAttr = [](const auto & targetArray, std::string_view name) -> const fastgltf::Attribute* {
						for (const auto& attr : targetArray) {
							if (attr.name == name) return &attr;
						}

						return nullptr;
					};

					if (auto* firstPos = findTargetAttr(inPrim.targets[0], "POSITION")) {
						const auto& acc = asset.accessors[firstPos->accessorIndex];

						if (acc.componentType == fastgltf::ComponentType::Byte || acc.componentType == fastgltf::ComponentType::UnsignedByte) {
							outPrim.meta.morphFormat = 2;
						} else if (acc.componentType == fastgltf::ComponentType::Short || acc.componentType == fastgltf::ComponentType::UnsignedShort) {
							outPrim.meta.morphFormat = 1;
						}
					}

					size_t bytesPerComponent = (outPrim.meta.morphFormat == 2) ? 1 : ((outPrim.meta.morphFormat == 1) ? 2 : 4);
					size_t targetByteSize = vertexCount * bytesPerComponent * 3;

					outPrim.morphBytes.resize(outPrim.meta.targetCount * targetByteSize * 2, 0);
					uint8_t* writePtr = outPrim.morphBytes.data();

					for (uint32_t t = 0; t < outPrim.meta.targetCount; ++t) {
						const auto& target = inPrim.targets[t];

						if (auto* posTarget = findTargetAttr(target, "POSITION")) {
							const auto& acc = asset.accessors[posTarget->accessorIndex];

							if (outPrim.meta.morphFormat == 2) {
								fastgltf::copyFromAccessor<std::array<int8_t, 3>>(asset, acc, writePtr);
							} else if (outPrim.meta.morphFormat == 1) {
								fastgltf::copyFromAccessor<std::array<int16_t, 3>>(asset, acc, writePtr);
							} else {
								fastgltf::copyFromAccessor<std::array<float, 3>>(asset, acc, writePtr);
							}
						}

						writePtr += targetByteSize;

						if (auto* normTarget = findTargetAttr(target, "NORMAL")) {
							const auto& acc = asset.accessors[normTarget->accessorIndex];

							if (outPrim.meta.morphFormat == 2) {
								fastgltf::copyFromAccessor<std::array<int8_t, 3>>(asset, acc, writePtr);
							} else if (outPrim.meta.morphFormat == 1) {
								fastgltf::copyFromAccessor<std::array<int16_t, 3>>(asset, acc, writePtr);
							} else {
								fastgltf::copyFromAccessor<std::array<float, 3>>(asset, acc, writePtr);
							}
						}

						writePtr += targetByteSize;
					}
				}
			}

			staging.meshes[i].primitives.push_back(std::move(outPrim));
		}
	}

	if (!asset.skins.empty() && !asset.scenes.empty()) {
		staging.isSkinned = true;
		staging.parsed_skel = std::make_unique<ParsedSkeleton>();
		AssetBaker::BakeSkeleton(asset, *staging.parsed_skel, 0);

		std::vector<glm::mat4> savedRestPose(staging.parsed_skel->nodeCount);
		std::memcpy(savedRestPose.data(), staging.parsed_skel->localTransforms.get(), staging.parsed_skel->nodeCount * sizeof(glm::mat4));

		gltfnodedata nodedata{};
		int rootNodeNum = asset.scenes.at(0).nodeIndices.at(0);
		nodedata.rootnode = vknode::createroot(rootNodeNum);

		std::function<void(std::shared_ptr<vknode>)> build_nodedata = [&](std::shared_ptr<vknode> treeNode) {
			int nodeNum = treeNode->getnum();
			const fastgltf::Node &node = asset.nodes.at(nodeNum);
			treeNode->setname(static_cast<std::string>(node.name));
			std::visit(fastgltf::visitor{
				[](auto & arg) {},
				[&](fastgltf::TRS trs) {
					treeNode->settranslation(glm::make_vec3(trs.translation.data()));
					treeNode->setrotation(glm::make_quat(trs.rotation.data()));
					treeNode->setscale(glm::make_vec3(trs.scale.data()));
				}}, node.transform);
			treeNode->calculatenodemat();
		};

		std::function<void(std::shared_ptr<vknode>)> build_nodes = [&](std::shared_ptr<vknode> treeNode) {
			int nodeNum = treeNode->getnum();
			const auto &childNodes = asset.nodes.at(nodeNum).children;
			treeNode->addchildren(childNodes);

			for (auto &childNode : treeNode->getchildren()) {
				build_nodedata(childNode);
				build_nodes(childNode);
			}
		};

		build_nodedata(nodedata.rootnode);
		build_nodes(nodedata.rootnode);

		nodedata.nodelist.reserve(asset.nodes.size());
		nodedata.nodelist.resize(asset.nodes.size());
		nodedata.nodelist.at(rootNodeNum) = nodedata.rootnode;

		std::function<void(std::vector<std::shared_ptr<vknode>>&, int)> build_nodelist = [&](std::vector<std::shared_ptr<vknode>>& nodeList, int nodeNum) {
			for (auto &childNode : nodeList.at(nodeNum)->getchildren()) {
				int childNodeNum = childNode->getnum();
				nodeList.at(childNodeNum) = childNode;
				build_nodelist(nodeList, childNodeNum);
			}
		};
		build_nodelist(nodedata.nodelist, rootNodeNum);

		std::vector<bool> allTrueMask(nodedata.nodelist.size(), true);

		for (auto& anim0 : asset.animations) {
			std::shared_ptr<vkclip> clip0 = std::make_shared<vkclip>(static_cast<std::string>(anim0.name));

			for (auto& c : anim0.channels) {
				clip0->addchan(asset, anim0, c);
			}

			DODAnimationClip baked{};
			baked.name = clip0->getName();
			baked.duration = clip0->getEndTime();
			baked.sampleRate = rview::anim::samplerate;
			baked.nodeCount = staging.parsed_skel->nodeCount;
			baked.jointCount = staging.parsed_skel->jointCount;
			baked.frameCount = static_cast<uint32_t>(baked.duration * baked.sampleRate) + 1;

			baked.globalTransforms.resize(baked.frameCount * baked.nodeCount, glm::mat4(1.0f));
			baked.finalJointMatrices.resize(baked.frameCount * baked.jointCount, glm::mat4(1.0f));
			baked.finalJointDQs.resize(baked.frameCount * baked.jointCount, DualQuatScale());
			baked.localTracks.resize(baked.frameCount * baked.nodeCount);


			for (uint32_t f = 0; f < baked.frameCount; ++f) {
				float t = static_cast<float>(f) / baked.sampleRate;

				clip0->setFrame(nodedata.nodelist, allTrueMask, t);

				for (uint32_t n = 0; n < nodedata.nodelist.size(); ++n) {
					if (n >= MAX_BONES) continue;

					int32_t topoIdx = staging.parsed_skel->gltfToTopoMap[n];

					if (topoIdx >= 0 && topoIdx < static_cast<int32_t>(baked.nodeCount)) {
						if (nodedata.nodelist[n]) {

							staging.parsed_skel->localTransforms[topoIdx] = nodedata.nodelist[n]->getlocalmatrix();

							LocalTRS trs;
							trs.T = nodedata.nodelist[n]->getblendtrans();
							trs.R = nodedata.nodelist[n]->getblendrot();
							trs.S = nodedata.nodelist[n]->getblendscale();
							baked.localTracks[(f * baked.nodeCount) + topoIdx] = trs;
						}
					}
				}

				staging.parsed_skel->UpdateGlobalMatrices();

				uint32_t globalOffset = f * baked.nodeCount;
				uint32_t jointOffset = f * baked.jointCount;

				std::memcpy(&baked.globalTransforms[globalOffset],
				            staging.parsed_skel->globalTransforms.get(),
				            baked.nodeCount * sizeof(glm::mat4));

				for (uint32_t j = 0; j < baked.jointCount; ++j) {
					baked.finalJointDQs[jointOffset + j] = Mat4ToDualQuatScale(staging.parsed_skel->finalJointMatrices[j]);
				}
			}

			staging.bakedClips.push_back(std::move(baked));
		}

		std::memcpy(staging.parsed_skel->localTransforms.get(), savedRestPose.data(), staging.parsed_skel->nodeCount * sizeof(glm::mat4));
		staging.parsed_skel->UpdateGlobalMatrices();
	}

	return staging;
}

uint32_t commit_staging_to_vulkan(rvkbucket& mvkobjs, VkCommandBuffer cmd, StagingModelData& staging,
                                  std::vector<uint32_t>& uploadedTextureBindlessIDs) {


	std::vector<texdata> model_textures;
	std::vector<VkDescriptorImageInfo> imgInfos;
	std::vector<uint32_t> textureBindlessIndices;

	for (size_t i = 0; i < staging.textures.size(); ++i) {
		auto& st = staging.textures[i];

		if (st.pixels.empty()) continue;

		texdata tex = rview::rvk::tex::upload_texture_async(mvkobjs, cmd, st.pixels.data(), st.width, st.height);

		uint32_t bindlessID = rview::core::globalTextureCounter.fetch_add(1, std::memory_order_relaxed);
		uploadedTextureBindlessIDs[i] = bindlessID;

		VkDescriptorImageInfo descInfo{};
		descInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descInfo.imageView = tex.imgview;
		descInfo.sampler = mvkobjs.samplerz[0];

		imgInfos.push_back(descInfo);
		textureBindlessIndices.push_back(bindlessID);
		model_textures.push_back(tex);
	}

	if (!imgInfos.empty()) {
		std::vector<VkWriteDescriptorSet> writes(imgInfos.size());

		for (size_t i = 0; i < imgInfos.size(); ++i) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = rview::core::globalBindlessSet;
			writes[i].dstBinding = 0;
			writes[i].dstArrayElement = textureBindlessIndices[i];
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].descriptorCount = 1;
			writes[i].pImageInfo = &imgInfos[i];
		}

		std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}


	std::vector<uint32_t> globalMaterialIds;
	globalMaterialIds.reserve(staging.materials.size());

	{
		std::lock_guard<std::mutex> lock(rview::core::global_materials.mtx);
		MaterialData* global_array = static_cast<MaterialData*>(rview::core::global_materials.mapped_data);

		for (auto& sm : staging.materials) {
			uint32_t global_slot = rview::core::global_materials.free_slots.empty() ? 0 : rview::core::global_materials.free_slots.front();

			if (!rview::core::global_materials.free_slots.empty()) rview::core::global_materials.free_slots.pop();

			sm.data.albedoIdx   = (sm.localAlbedoImgIdx != 0xFFFFFFFF)   ? uploadedTextureBindlessIDs[sm.localAlbedoImgIdx]   : 1;
			sm.data.normalIdx   = (sm.localNormalImgIdx != 0xFFFFFFFF)   ? uploadedTextureBindlessIDs[sm.localNormalImgIdx]   : 2;
			sm.data.ormIdx      = (sm.localOrmImgIdx != 0xFFFFFFFF)      ? uploadedTextureBindlessIDs[sm.localOrmImgIdx]      : 1;
			sm.data.emissiveIdx = (sm.localEmissiveImgIdx != 0xFFFFFFFF) ? uploadedTextureBindlessIDs[sm.localEmissiveImgIdx] : 3;

			if (sm.isMToon) {
				sm.data.baseColorFactor *= glm::vec4(sm.mtoonShadeColorFactor, 1.0f);
			}

			global_array[global_slot] = sm.data;
			globalMaterialIds.push_back(global_slot);
		}
	}

	GpuBuffer modelVbo{};
	VkDeviceSize beltOffset = 0;

	if (staging.strictAlignedTotalBytes > 0) {
		vkvbo::init(mvkobjs, modelVbo, staging.strictAlignedTotalBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		beltOffset = mvkobjs.sbelts[rview::core::currentFrame].reserve(staging.strictAlignedTotalBytes);
	}

	std::vector<PrimitiveMetadata> finalPrimitives;
	std::vector<uint8_t> localRawIndices;
	std::vector<uint8_t> localMorphBytes;
	uint32_t totalPrimitiveCount = 0;
	VkDeviceSize currentBufferOffset = 0;

	std::vector<VkDescriptorBufferInfo> bufferInfos;
	std::vector<uint32_t> bIndices;
	bufferInfos.reserve(staging.meshes.size() * 10 * 6);
	bIndices.reserve(staging.meshes.size() * 10 * 6);

	for (auto& mesh : staging.meshes) {
		for (auto& prim : mesh.primitives) {
			totalPrimitiveCount++;

			uint32_t localMatIdx = prim.meta.materialID;
			prim.meta.materialID = (localMatIdx != 0xFFFFFFFF && localMatIdx < globalMaterialIds.size())
			                       ? globalMaterialIds[localMatIdx]
			                       : (globalMaterialIds.empty() ? 0 : globalMaterialIds[0]);

			for (int slot = 0; slot < 6; ++slot) {
				if (prim.vertexBytes[slot].empty()) continue;

				size_t byteSize = prim.vertexBytes[slot].size();
				size_t alignedSize = (byteSize + 255) & ~255;

				std::memcpy(mvkobjs.sbelts[rview::core::currentFrame].mappedData + beltOffset + currentBufferOffset, prim.vertexBytes[slot].data(), byteSize);

				uint32_t bIdx = rview::core::globalBufferCounter.fetch_add(1, std::memory_order_relaxed);

				bufferInfos.push_back({modelVbo.buffer, currentBufferOffset, byteSize});
				bIndices.push_back(bIdx);

				if (slot == 0) prim.meta.posIdx = bIdx;
				else if (slot == 1) prim.meta.norIdx = bIdx;
				else if (slot == 2) prim.meta.tanIdx = bIdx;
				else if (slot == 3) prim.meta.texIdx  = bIdx;
				else if (slot == 4) prim.meta.joiIdx = bIdx;
				else if (slot == 5) prim.meta.weiIdx = bIdx;

				currentBufferOffset += alignedSize;
			}

			prim.meta.indexByteOffset = static_cast<uint32_t>(localRawIndices.size());

			if (!prim.indexBytes.empty()) {
				uint32_t rawSize = prim.indexBytes.size();
				uint32_t paddedSize = (rawSize + 3) & ~3;

				size_t oldSize = localRawIndices.size();
				localRawIndices.resize(oldSize + paddedSize, 0);
				std::memcpy(localRawIndices.data() + oldSize, prim.indexBytes.data(), rawSize);
			}

			if (!prim.morphBytes.empty()) {
				prim.meta.morphDeltaIdx = static_cast<uint32_t>(localMorphBytes.size());
				localMorphBytes.insert(localMorphBytes.end(), prim.morphBytes.begin(), prim.morphBytes.end());
			} else {
				prim.meta.morphDeltaIdx = 0xFFFFFFFF;
			}

			finalPrimitives.push_back(prim.meta);
		}
	}

	if (!bufferInfos.empty()) {
		std::vector<VkWriteDescriptorSet> writes(bufferInfos.size());

		for (size_t i = 0; i < bufferInfos.size(); ++i) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = rview::core::globalBindlessSet;
			writes[i].dstBinding = 5;
			writes[i].dstArrayElement = bIndices[i];
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[i].descriptorCount = 1;
			writes[i].pBufferInfo = &bufferInfos[i];
		}

		std::lock_guard<std::shared_mutex> lock(*rview::core::mtx2);
		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	if (staging.strictAlignedTotalBytes > 0) {
		vmaFlushAllocation(mvkobjs.alloc, mvkobjs.sbelts[rview::core::currentFrame].allocation, beltOffset, staging.strictAlignedTotalBytes);

		VkBufferCopy region{beltOffset, 0, staging.strictAlignedTotalBytes};
		vkCmdCopyBuffer(cmd, mvkobjs.sbelts[rview::core::currentFrame].buffer, modelVbo.buffer, 1, &region);

		VkBufferMemoryBarrier2 b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
		b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		b.buffer = modelVbo.buffer;
		b.size = VK_WHOLE_SIZE;

		VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
		dep.bufferMemoryBarrierCount = 1;
		dep.pBufferMemoryBarriers = &b;
		vkCmdPipelineBarrier2(cmd, &dep);
	}

	uint32_t assignedModelID = 0;
	{
		std::lock_guard<std::mutex> lock(g_assets.registryMutex);
		assignedModelID = static_cast<uint32_t>(g_assets.models.size());

		uint32_t globalByteOffset = static_cast<uint32_t>(g_assets.globalRawIndices.size());
		uint32_t globalMorphOffset = static_cast<uint32_t>(g_assets.globalMorphBytes.size());

		glm::vec3 modelMin(std::numeric_limits<float>::max());
		glm::vec3 modelMax(std::numeric_limits<float>::lowest());

		for (auto& meta : finalPrimitives) {
			glm::vec3 primMin(meta.aabbMinX, meta.aabbMinY, meta.aabbMinZ);
			glm::vec3 primMax(meta.aabbMaxX, meta.aabbMaxY, meta.aabbMaxZ);
			modelMin = glm::min(modelMin, primMin);
			modelMax = glm::max(modelMax, primMax);
			meta.indexByteOffset += globalByteOffset;

			if (meta.morphDeltaIdx != 0xFFFFFFFF) {
				meta.morphDeltaIdx += globalMorphOffset;
			}

			g_assets.primitives.push_back(meta);
		}

		if (totalPrimitiveCount == 0) {
			modelMin = glm::vec3(0.0f);
			modelMax = glm::vec3(0.0f);
		}

		ModelMetadata newModel{};
		newModel.firstPrimitiveIndex = static_cast<uint32_t>(g_assets.primitives.size() - totalPrimitiveCount);
		newModel.primitiveCount = totalPrimitiveCount;
		newModel.setAABB(modelMin, modelMax);

		g_assets.models.push_back(newModel);

		g_assets.globalRawIndices.insert(
		    g_assets.globalRawIndices.end(),
		    localRawIndices.begin(),
		    localRawIndices.end()
		);

		g_assets.globalMorphBytes.insert(
		    g_assets.globalMorphBytes.end(),
		    localMorphBytes.begin(),
		    localMorphBytes.end()
		);

		for (auto& meta : finalPrimitives) {
			meta.indexByteOffset += globalByteOffset;

			if (meta.morphDeltaIdx != 0xFFFFFFFF) {
				meta.morphDeltaIdx += globalMorphOffset;
			}

			g_assets.primitives.push_back(meta);
		}
	}

	{
		std::lock_guard<std::mutex> resLock(g_modelResourceMtx);
		g_modelVBOs[assignedModelID] = modelVbo;
		CPUModelAsset cpuAsset;
		cpuAsset.isSkinned = staging.isSkinned;
		cpuAsset.geometryVBO = modelVbo;
		cpuAsset.bakedClips = std::move(staging.bakedClips);
		cpuAsset.textures = std::move(model_textures);
		cpuAsset.materialIDs = globalMaterialIds;
		g_cpuModels[assignedModelID] = std::move(cpuAsset);
		g_model_filepaths[assignedModelID] = staging.name;
	}

	g_assets.requiresUpload.store(true, std::memory_order_release);

	return assignedModelID;
}
inline void EvaluateFK_And_CompressDQS(
    uint32_t b_count, uint32_t j_count, uint32_t b_start, uint32_t j_start,
    const LocalTRS* locals) {
	for (uint32_t b = 0; b < b_count; ++b) {
		glm::mat4 localMat = glm::translate(glm::mat4(1.0f), locals[b].T) * glm::mat4_cast(locals[b].R) * glm::scale(glm::mat4(1.0f), locals[b].S);

		int32_t parent = g_bone_parents[b_start + b];

		if (parent >= 0) g_bone_globals[b_start + b] = g_bone_globals[parent] * localMat;
		else             g_bone_globals[b_start + b] = localMat;
	}

	for (uint32_t j = 0; j < j_count; ++j) {
		uint32_t globalNodeIdx = g_joint_to_node[j_start + j];
		glm::mat4 finalMat = g_bone_globals[globalNodeIdx] * g_joint_inverse_binds[j_start + j];
		g_joint_final_matrices[j_start + j] = Mat4ToDualQuatScale(finalMat);
	}
}
void update_logic_and_animations(float deltaTime) {
	ZoneScoped;
	uint32_t active_instances = g_scene.entity_count.load(std::memory_order_relaxed);

	for (uint32_t i = 0; i < active_instances; ++i) {
		if (!g_registry.is_visible[i] || g_registry.anim_mode[i] != AnimMode::Baked) continue;

		uint32_t j_start = g_registry.joint_start_index[i];
		uint32_t j_count = g_registry.joint_count[i];

		if (j_count == 0 || g_scene.modelIDs[i] == 0xFFFFFFFF) continue;

		auto it = g_cpuModels.find(g_scene.modelIDs[i]);

		if (it != g_cpuModels.end()) {
			int clipIdx = g_registry.anim_clip[i];

			if (clipIdx == -1) {
				DualQuatScale identityDQ;
				identityDQ.real  = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
				identityDQ.dual  = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
				identityDQ.scale = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);

				for (uint32_t j = 0; j < j_count; ++j) {
					g_joint_final_matrices[j_start + j] = identityDQ;
				}
			} else if (!it->second.bakedClips.empty() && clipIdx >= 0 && clipIdx < it->second.bakedClips.size()) {
				g_scene.animTimePositions[i] += deltaTime * g_registry.anim_speed[i];
				const auto& clip = it->second.bakedClips[clipIdx];
				const DualQuatScale* frameJoints = clip.GetFinalJointsFrame(g_scene.animTimePositions[i], g_registry.anim_loop[i]);

				if (frameJoints) {
					std::memcpy(&g_joint_final_matrices[j_start], frameJoints, j_count * sizeof(DualQuatScale));
				}
			}
		}
	}

	for (uint32_t i = 0; i < active_instances; ++i) {
		AnimMode mode = g_registry.anim_mode[i];

		if (!g_registry.is_visible[i] || (mode != AnimMode::Dynamic && mode != AnimMode::Hero && mode != AnimMode::Hybrid)) continue;

		uint32_t b_start = g_registry.bone_start_index[i];
		uint32_t b_count = g_registry.bone_count[i];
		uint32_t j_start = g_registry.joint_start_index[i];
		uint32_t j_count = g_registry.joint_count[i];

		if (b_count == 0 || g_scene.modelIDs[i] == 0xFFFFFFFF) continue;

		auto it = g_cpuModels.find(g_scene.modelIDs[i]);

		if (it == g_cpuModels.end() || it->second.bakedClips.empty()) continue;

		g_scene.animTimePositions[i] += deltaTime * g_registry.anim_speed[i];

		int currentClipIdx = g_registry.anim_clip[i];
		int targetClipIdx = g_registry.target_anim_clip[i];

		const LocalTRS* tracksA = nullptr;

		if (currentClipIdx >= 0 && currentClipIdx < it->second.bakedClips.size()) {
			const auto& clipA = it->second.bakedClips[currentClipIdx];
			tracksA = clipA.GetLocalTracksFrame(g_scene.animTimePositions[i], g_registry.anim_loop[i]);
		}

		LocalTRS locals[MAX_BONES];
		float blendWeight = 0.0f;
		const LocalTRS* tracksB = nullptr;

		if (targetClipIdx >= 0 && targetClipIdx < it->second.bakedClips.size()) {
			g_registry.current_blend_time[i] += deltaTime;
			blendWeight = glm::clamp(g_registry.current_blend_time[i] / g_registry.blend_duration[i], 0.0f, 1.0f);

			const auto& clipB = it->second.bakedClips[targetClipIdx];
			tracksB = clipB.GetLocalTracksFrame(g_registry.current_blend_time[i], true);

			if (blendWeight >= 1.0f) {
				g_registry.anim_clip[i] = targetClipIdx;
				g_registry.target_anim_clip[i] = -1;
				g_scene.animTimePositions[i] = g_registry.current_blend_time[i];
				g_registry.current_blend_time[i] = 0.0f;
				blendWeight = 0.0f;
				tracksB = nullptr;
			}
		}

		float magnitude = g_registry.anim_magnitude[i];

		for (uint32_t b = 0; b < b_count; ++b) {
			LocalTRS baseTrack;
			float totalNWayWeight = 0.0f;

			glm::vec3 accumT(0.0f);
			glm::vec3 accumS(0.0f);
			glm::quat accumR = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

			for (uint32_t layer = 0; layer < Registry::MAX_BLEND_LAYERS; ++layer) {
				int clipIdx = g_registry.blend_clips[i][layer];
				float weight = g_registry.blend_weights[i][layer];

				if (clipIdx >= 0 && weight > 0.0f) {
					const auto& layerClip = it->second.bakedClips[clipIdx];
					const LocalTRS* layerTracks = layerClip.GetLocalTracksFrame(g_scene.animTimePositions[i], true);

					if (layerTracks) {
						accumT += layerTracks[b].T * weight;
						accumS += layerTracks[b].S * weight;

						if (totalNWayWeight == 0.0f) {
							accumR = layerTracks[b].R;
						} else {
							float normalizedWeight = weight / (totalNWayWeight + weight);
							accumR = glm::slerp(accumR, layerTracks[b].R, normalizedWeight);
						}

						totalNWayWeight += weight;
					}
				}
			}

			if (totalNWayWeight > 0.0f) {
				baseTrack.T = accumT / totalNWayWeight;
				baseTrack.S = accumS / totalNWayWeight;
				baseTrack.R = accumR;
			} else if (tracksA) {
				baseTrack = tracksA[b];
			} else {
				glm::mat4 bindMat = g_bone_locals[b_start + b];
				baseTrack.T = glm::vec3(bindMat[3]);
				baseTrack.S = glm::vec3(
				                  glm::length(glm::vec3(bindMat[0])),
				                  glm::length(glm::vec3(bindMat[1])),
				                  glm::length(glm::vec3(bindMat[2]))
				              );
				glm::mat3 rotMat(
				    baseTrack.S.x > 0.0001f ? glm::vec3(bindMat[0]) / baseTrack.S.x : glm::vec3(1, 0, 0),
				    baseTrack.S.y > 0.0001f ? glm::vec3(bindMat[1]) / baseTrack.S.y : glm::vec3(0, 1, 0),
				    baseTrack.S.z > 0.0001f ? glm::vec3(bindMat[2]) / baseTrack.S.z : glm::vec3(0, 0, 1)
				);
				baseTrack.R = glm::quat_cast(rotMat);
			}

			float magnitude = g_registry.anim_magnitude[i];

			if (magnitude != 1.0f) {
				glm::mat4 bindMat = g_bone_locals[b_start + b];

				glm::vec3 restT = glm::vec3(bindMat[3]);
				glm::vec3 restS = glm::vec3(
				                      glm::length(glm::vec3(bindMat[0])),
				                      glm::length(glm::vec3(bindMat[1])),
				                      glm::length(glm::vec3(bindMat[2]))
				                  );

				glm::mat3 rotMat(
				    restS.x > 0.0001f ? glm::vec3(bindMat[0]) / restS.x : glm::vec3(1, 0, 0),
				    restS.y > 0.0001f ? glm::vec3(bindMat[1]) / restS.y : glm::vec3(0, 1, 0),
				    restS.z > 0.0001f ? glm::vec3(bindMat[2]) / restS.z : glm::vec3(0, 0, 1)
				);
				glm::quat restR = glm::quat_cast(rotMat);

				baseTrack.T = glm::mix(restT, baseTrack.T, magnitude);
				baseTrack.S = glm::mix(restS, baseTrack.S, magnitude);

				if (glm::dot(restR, baseTrack.R) < 0.0f) {
					baseTrack.R = -baseTrack.R;
				}

				baseTrack.R = glm::normalize(glm::slerp(restR, baseTrack.R, magnitude));
			}

			// just a prototype
			if (b == 0 || b == 1) {
				extern float g_app_time; // maybe ill use g_scene.animTimePositions[i] idk

				// TEMP
				float speed = 2.0f;
				float amp = 0.02f;

				float breathCycle = sin(g_scene.animTimePositions[i] * speed);

				glm::vec3 breathScale = glm::vec3(
				                            1.0f,
				                            1.0f + (breathCycle * amp),
				                            1.0f + (breathCycle * amp * 1.5f)
				                        );

				baseTrack.S *= breathScale;
			}

			if (blendWeight > 0.0f && tracksB) {
				locals[b].T = glm::mix(baseTrack.T, tracksB[b].T, blendWeight);
				locals[b].S = glm::mix(baseTrack.S, tracksB[b].S, blendWeight);
				locals[b].R = glm::slerp(baseTrack.R, tracksB[b].R, blendWeight);
			} else {
				locals[b] = baseTrack;
			}
		}

		if (mode == AnimMode::Hero || mode == AnimMode::Hybrid) {
			uint32_t active_chains = g_registry.ik_active_chains[i];

			for (uint32_t ik_idx = 0; ik_idx < active_chains; ++ik_idx) {
				int effector = g_registry.ik_effector_node[i][ik_idx];
				int root = g_registry.ik_root_node[i][ik_idx];
				glm::vec3 target = g_registry.ik_target_pos[i][ik_idx];

				if (effector < 0 || root < 0) continue;

				int chain[MAX_BONES];
				int chain_len = 0;
				int curr = effector;

				while (curr >= 0) {
					chain[chain_len++] = curr;

					if (curr == root) break;

					curr = g_bone_parents[b_start + curr];

					if (curr >= 0) curr -= b_start;
				}

				for (int iter = 0; iter < 10; ++iter) {
					for (uint32_t b = 0; b < b_count; ++b) {
						glm::mat4 localMat = glm::translate(glm::mat4(1.0f), locals[b].T) * glm::mat4_cast(locals[b].R) * glm::scale(glm::mat4(1.0f), locals[b].S);
						int32_t parent = g_bone_parents[b_start + b];

						if (parent >= 0) g_bone_globals[b_start + b] = g_bone_globals[parent] * localMat;
						else             g_bone_globals[b_start + b] = localMat;
					}

					glm::vec3 effector_pos = glm::vec3(g_bone_globals[b_start + effector][3]);

					if (glm::length(target - effector_pos) < 0.001f) break;

					for (int c = 0; c < chain_len; ++c) {
						int b = chain[c];
						glm::vec3 node_pos = glm::vec3(g_bone_globals[b_start + b][3]);

						glm::vec3 to_effector = effector_pos - node_pos;
						glm::vec3 to_target = target - node_pos;

						if (glm::length(to_effector) < 0.001f || glm::length(to_target) < 0.001f) continue;

						to_effector = glm::normalize(to_effector);
						to_target = glm::normalize(to_target);

						if (glm::dot(to_effector, to_target) > 0.9999f) continue;

						glm::quat delta_global = glm::rotation(to_effector, to_target);

						glm::quat parent_global_rot{1.0f, 0.0f, 0.0f, 0.0f};
						int p = g_bone_parents[b_start + b];

						if (p >= 0) {
							glm::mat3 pMat(g_bone_globals[p]);
							pMat[0] = glm::normalize(pMat[0]);
							pMat[1] = glm::normalize(pMat[1]);
							pMat[2] = glm::normalize(pMat[2]);
							parent_global_rot = glm::quat_cast(pMat);
						}

						glm::quat delta_local = glm::inverse(parent_global_rot) * delta_global * parent_global_rot;
						locals[b].R = glm::normalize(delta_local * locals[b].R);

						for (int k = c; k >= 0; --k) {
							int child = chain[k];
							int child_p = g_bone_parents[b_start + child];
							glm::mat4 child_local = glm::translate(glm::mat4(1.0f), locals[child].T) * glm::mat4_cast(locals[child].R) * glm::scale(glm::mat4(1.0f),
							                        locals[child].S);

							if (child_p >= 0) g_bone_globals[b_start + child] = g_bone_globals[child_p] * child_local;
							else              g_bone_globals[b_start + child] = child_local;
						}

						effector_pos = glm::vec3(g_bone_globals[b_start + effector][3]);
					}
				}
			}
		}

		EvaluateFK_And_CompressDQS(b_count, j_count, b_start, j_start, locals);
	}

	for (uint32_t i = 0; i < active_instances; ++i) {
		if (!g_registry.is_visible[i]) continue;

		uint32_t parent_idx = g_registry.attach_parent_entity[i];
		int32_t parent_bone = g_registry.attach_parent_bone[i];

		if (parent_idx != 0xFFFFFFFF && parent_bone >= 0) {

			glm::mat4 bone_local_mat = get_bone_matrix(parent_idx, parent_bone);

			glm::mat4 offset_mat = glm::translate(glm::mat4(1.0f), g_registry.attach_offset_pos[i]) * glm::mat4_cast(g_registry.attach_offset_rot[i]);

			glm::mat4 final_attachment_mat = bone_local_mat * offset_mat;

			glm::vec3 parent_world_pos = g_scene.worldPositions[parent_idx];
			glm::quat parent_world_rot = g_scene.rotations[parent_idx];
			glm::vec3 parent_scale     = g_scene.scales[parent_idx];

			glm::mat4 parent_world_mat = glm::translate(glm::mat4(1.0f), parent_world_pos) * glm::mat4_cast(parent_world_rot) * glm::scale(glm::mat4(1.0f),
			                             parent_scale);

			glm::mat4 absolute_world_mat = parent_world_mat * final_attachment_mat;

			g_scene.worldPositions[i] = glm::vec3(absolute_world_mat[3]);

			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(absolute_world_mat, scale, rotation, translation, skew, perspective);

			g_scene.rotations[i] = glm::conjugate(rotation);
			g_scene.scales[i] = scale;
		}
	}
}

void upload_joint_matrices(rvkbucket& mvkobjs) {
	if (g_joint_final_matrices.empty()) return;

	size_t upload_bytes = g_joint_final_matrices.size() * sizeof(DualQuatScale);
	uint32_t frame_idx = rview::core::currentFrame;

	if (upload_bytes > g_gpu_joint_buffers[frame_idx].size) {
		std::cerr << "CRITICAL: Joint Mega-Buffer overflowed GPU capacity!\n";
		return;
	}

	void* mappedData;
	vmaMapMemory(mvkobjs.alloc, g_gpu_joint_buffers[frame_idx].alloc, &mappedData);

	std::memcpy(mappedData, g_joint_final_matrices.data(), upload_bytes);

	vmaFlushAllocation(mvkobjs.alloc, g_gpu_joint_buffers[frame_idx].alloc, 0, upload_bytes);
	vmaUnmapMemory(mvkobjs.alloc, g_gpu_joint_buffers[frame_idx].alloc);
}


void init_gpu_joint_buffers(rvkbucket& mvkobjs) {
	g_gpu_joint_buffers.resize(rview::core::MAX_FRAMES_IN_FLIGHT);

	size_t bufferSize = 250000 * sizeof(DualQuatScale);

	for (uint32_t i = 0; i < rview::core::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		binfo.size = bufferSize;
		binfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		VmaAllocationCreateInfo vmaallocinfo{};
		vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		vmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		if (vmaCreateBuffer(mvkobjs.alloc, &binfo, &vmaallocinfo,
		                    &g_gpu_joint_buffers[i].buffer,
		                    &g_gpu_joint_buffers[i].alloc, nullptr) != VK_SUCCESS) {
			std::cerr << "Fatal: Failed to allocate GPU joint buffer!\n";
			return;
		}

		g_gpu_joint_buffers[i].size = bufferSize;

		VkDescriptorBufferInfo ssboInfo{g_gpu_joint_buffers[i].buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = rview::core::globalBindlessSet;
		write.dstBinding = 2;
		write.dstArrayElement = i;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &ssboInfo;

		vkUpdateDescriptorSets(mvkobjs.vkdevice.device, 1, &write, 0, nullptr);
	}
}

}