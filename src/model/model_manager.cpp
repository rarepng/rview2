#include <model_manager.hpp>
#include <vkvbo.hpp>
#include <fstream>
#include <simdjson.h>
#include <unordered_map>
#include <iostream>
#include <stb_image.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <anim/anim2.hpp>

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

StagingModelData parse_model_to_staging(std::string_view filepath, uint32_t dropID) {
	StagingModelData staging;
	staging.name = filepath;
	staging.dropID = dropID;

	if (dropID != 0xFFFFFFFF) {
		ProgressUpdate update;
		update.requestID = dropID;
		update.step = ParseStep::parsing;
		strncpy(update.message, filepath.data(), sizeof(update.message) - 1);
		update.message[sizeof(update.message) - 1] = '\0';
		g_progress_queue.push(update);
	}

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

		// staging.bakedClips.resize(std::max<size_t>(10, asset.animations.size())); // TODO fix the magic 10 (from anim too)!!!!!

		staging.parsedPools.resize(asset.animations.size());

		if (!asset.animations.empty()) {
			std::atomic<int> pending_clips{static_cast<int>(asset.animations.size())};
			std::vector<bool> allTrueMask(asset.nodes.size(), true);

			struct BakeContext {
				StagingModelData* staging;
				std::vector<bool>* mask;
				std::atomic<int>* pending_clips;
				fastgltf::Asset* asset;
				uint32_t dropID;
			};

			BakeContext ctx { &staging, &allTrueMask, &pending_clips, &asset, dropID };
			BakeContext* pCtx = &ctx;

			if (dropID != 0xFFFFFFFF) {
				ProgressUpdate bakeUpdate;
				bakeUpdate.requestID = dropID;
				bakeUpdate.step = ParseStep::baking;
				strncpy(bakeUpdate.message, "Baking Animations...", sizeof(bakeUpdate.message) - 1);
				g_progress_queue.push(bakeUpdate);
			}

			for (size_t animIdx = 0; animIdx < asset.animations.size(); ++animIdx) {
				uint32_t aIdx = static_cast<uint32_t>(animIdx);

				g_jobs.enqueue([pCtx, aIdx]() {
					ScopedJobGuard guard(*pCtx->pending_clips);

					if (!g_jobs.is_running() ||
					        (pCtx->dropID != 0xFFFFFFFF && g_cancellation_tokens[pCtx->dropID].load(std::memory_order_relaxed))) {
						return;
					}

					const auto& anim0 = pCtx->asset->animations[aIdx];

					ThreadLocalArena isolatedArena;
					isolatedArena.init(128 * 1024 * 1024);

					DODRawClip rawClip = extract_raw_clip(isolatedArena, *pCtx->asset, anim0);

					JobSkeletonState jobSkel;
					jobSkel.allocate_from_arena(isolatedArena, *pCtx->staging->parsed_skel);

					AnimPoolData baked{};
					baked.name = std::string(rawClip.name);
					baked.skeletonSignature = GenerateSkeletonSignature(*pCtx->staging->parsed_skel);
					baked.duration = rawClip.duration;
					baked.sampleRate = rview::anim::samplerate;
					baked.nodeCount = jobSkel.nodeCount;
					baked.jointCount = jobSkel.jointCount;

					baked.frameCount = static_cast<uint32_t>(baked.duration * baked.sampleRate) + 1;

					if (baked.duration <= 0.0f || baked.frameCount == 0) baked.frameCount = 1;

					baked.globalTransforms.resize(baked.frameCount * baked.nodeCount, glm::mat4(1.0f));
					baked.finalJointDQs.resize(baked.frameCount * baked.jointCount, DualQuatScale());
					baked.localTracks.resize(baked.frameCount * baked.nodeCount);

					std::vector<LocalTRS> restPoseTracks(baked.nodeCount);

					for (uint32_t n = 0; n < baked.nodeCount; ++n) {
						glm::mat4 restMat = pCtx->staging->parsed_skel->localTransforms[n];
						LocalTRS trs;
						trs.T = glm::vec3(restMat[3]);
						trs.S = glm::vec3(glm::length(glm::vec3(restMat[0])), glm::length(glm::vec3(restMat[1])), glm::length(glm::vec3(restMat[2])));

						glm::mat3 rotMat(
						    trs.S.x > 0.0001f ? glm::vec3(restMat[0]) / trs.S.x : glm::vec3(1, 0, 0),
						    trs.S.y > 0.0001f ? glm::vec3(restMat[1]) / trs.S.y : glm::vec3(0, 1, 0),
						    trs.S.z > 0.0001f ? glm::vec3(restMat[2]) / trs.S.z : glm::vec3(0, 0, 1)
						);
						trs.R = glm::quat_cast(rotMat);
						restPoseTracks[n] = trs;
					}

					uint32_t framesPerBatch = 64;
					uint32_t batchCount = (baked.frameCount + framesPerBatch - 1) / framesPerBatch;
					std::atomic<int> batchesPending{static_cast<int>(batchCount)};

					struct SubJobCtx {
						AnimPoolData* baked;
						BakeContext* pCtx;
						std::vector<LocalTRS>* restPoseTracks;
						DODRawClip* rawClip;
						std::atomic<int>* batchesPending;
						uint32_t framesPerBatch;
					} subCtx { &baked, pCtx, &restPoseTracks, &rawClip, &batchesPending, framesPerBatch };

					SubJobCtx* pSubCtx = &subCtx;

					for (uint32_t b = 0; b < batchCount; ++b) {
						g_jobs.enqueue([pSubCtx, b]() {
							ScopedJobGuard batchGuard(*pSubCtx->batchesPending);

							if (pSubCtx->pCtx->dropID != 0xFFFFFFFF &&
							        g_cancellation_tokens[pSubCtx->pCtx->dropID].load(std::memory_order_relaxed)) {
								return;
							}

							// pointer containment unit
							AnimPoolData& baked = *pSubCtx->baked;
							const DODRawClip& rawClip = *pSubCtx->rawClip;
							const std::vector<LocalTRS>& restPoseTracks = *pSubCtx->restPoseTracks;
							const ParsedSkeleton& skel = *pSubCtx->pCtx->staging->parsed_skel;
							const auto& mask = *pSubCtx->pCtx->mask;
							const uint32_t sampleRate = baked.sampleRate;

							std::vector<glm::mat4> localTransforms(baked.nodeCount);
							std::vector<glm::mat4> globalTransforms(baked.nodeCount);
							std::vector<glm::mat4> finalJointMatrices(baked.jointCount);
							std::vector<uint8_t> dirtyNodes(baked.nodeCount, 0);

							uint32_t startF = b * pSubCtx->framesPerBatch;
							uint32_t endF = std::min(startF + pSubCtx->framesPerBatch, baked.frameCount);

							for (uint32_t f = startF; f < endF; ++f) {

								// TOO COLD TO BE HERE
								// if (pSubCtx->pCtx->dropID != 0xFFFFFFFF &&
								// 	g_cancellation_tokens[pSubCtx->pCtx->dropID].load(std::memory_order_relaxed)) {
								// 	return;
								// }

								float t = static_cast<float>(f) / sampleRate;

								std::memcpy(&baked.localTracks[f * baked.nodeCount], restPoseTracks.data(), baked.nodeCount * sizeof(LocalTRS));
								std::memcpy(localTransforms.data(), skel.localTransforms.get(), baked.nodeCount * sizeof(glm::mat4));
								std::fill(dirtyNodes.begin(), dirtyNodes.end(), 0);

								for (uint32_t c = 0; c < rawClip.channelCount; ++c) {
									const DODRawChannel& chan = rawClip.channels[c];

									if (!mask[chan.targetNode]) continue;

									int32_t topoIdx = skel.gltfToTopoMap[chan.targetNode];

									if (topoIdx >= 0 && topoIdx < static_cast<int32_t>(baked.nodeCount)) {
										LocalTRS& trs = baked.localTracks[(f * baked.nodeCount) + topoIdx];

										if (chan.path == AnimPathType::Rotation) trs.R = chan.sample_quat(t);
										else if (chan.path == AnimPathType::Scale) trs.S = chan.sample_vec3(t);
										else if (chan.path == AnimPathType::Translation) trs.T = chan.sample_vec3(t);

										dirtyNodes[topoIdx] = 1;
									}
								}

								for (uint32_t n = 0; n < baked.nodeCount; ++n) {
									if (dirtyNodes[n]) {
										const LocalTRS& trs = baked.localTracks[(f * baked.nodeCount) + n];
										localTransforms[n] = glm::translate(glm::mat4(1.0f), trs.T) * glm::mat4_cast(trs.R) * glm::scale(glm::mat4(1.0f), trs.S);
									}
								}

								for (uint32_t i = 0; i < baked.nodeCount; ++i) {
									int32_t parent = skel.parentIndices[i];

									if (parent >= 0) {
										globalTransforms[i] = globalTransforms[parent] * localTransforms[i];
									} else {
										globalTransforms[i] = localTransforms[i];
									}
								}

								for (uint32_t j = 0; j < baked.jointCount; ++j) {
									uint32_t nodeIdx = skel.jointToNodeMap[j];
									finalJointMatrices[j] = globalTransforms[nodeIdx] * skel.inverseBindMatrices[j];
								}

								uint32_t globalOffset = f * baked.nodeCount;
								uint32_t jointOffset = f * baked.jointCount;
								std::memcpy(&baked.globalTransforms[globalOffset], globalTransforms.data(), baked.nodeCount * sizeof(glm::mat4));

								for (uint32_t j = 0; j < baked.jointCount; ++j) {
									baked.finalJointDQs[jointOffset + j] = Mat4ToDualQuatScale(finalJointMatrices[j]);
								}
							}
						});
					}

					while (batchesPending.load(std::memory_order_acquire) > 0) {
						if (!g_jobs.help_execute()) std::this_thread::yield();
					}

					pCtx->staging->parsedPools[aIdx] = std::move(baked);
				});
			}

			while (pending_clips.load(std::memory_order_acquire) > 0) {
				if (!g_jobs.help_execute()) {
					std::this_thread::yield();
				}
			}
		}

		if (dropID != 0xFFFFFFFF && !g_cancellation_tokens[dropID].load(std::memory_order_acquire)) {
			ProgressUpdate doneUpdate;
			doneUpdate.requestID = dropID;
			doneUpdate.step = ParseStep::done;
			strncpy(doneUpdate.message, "Done", sizeof(doneUpdate.message) - 1);
			g_progress_queue.push(doneUpdate);
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
	}

	std::vector<uint32_t> committedHandles;
	committedHandles.reserve(staging.parsedPools.size());

	for (auto& pool : staging.parsedPools) {
		if (pool.frameCount > 0) {
			uint32_t handle = g_animPoolRegistry.RegisterPool(std::move(pool));
			committedHandles.push_back(handle);
		}
	}

	{
		std::lock_guard<std::mutex> resLock(g_modelResourceMtx);
		g_modelVBOs[assignedModelID] = modelVbo;

		CPUModelAsset cpuAsset;
		cpuAsset.isSkinned = staging.isSkinned;
		cpuAsset.geometryVBO = modelVbo;
		cpuAsset.defaultClips = std::move(committedHandles);
		cpuAsset.textures = std::move(model_textures);
		cpuAsset.materialIDs = globalMaterialIds;

		g_cpuModels[assignedModelID] = std::move(cpuAsset);
		g_model_filepaths[assignedModelID] = staging.name;
	}

	g_assets.requiresUpload.store(true, std::memory_order_release);

	return assignedModelID;
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

	size_t bufferSize = rview::anim::maxjoints * sizeof(DualQuatScale);

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