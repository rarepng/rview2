
#define GLM_ENABLE_EXPERIMENTAL
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <iostream>

#include "vktexture.hpp"

#include "genericmodel.hpp"
#include "vkebo.hpp"
#include "vkvbo.hpp"

bool genericmodel::loadmodel(rvk &objs, std::string fname) {

	fastgltf::Parser fastparser{};
	auto buff = fastgltf::MappedGltfFile::FromPath(fname);
	auto a = fastparser.loadGltfBinary(buff.get(), std::filesystem::absolute(fname));
	mmodel2 = std::move(a.get());

	mgltfobjs.texs.reserve(mmodel2.images.size());
	mgltfobjs.texs.resize(mmodel2.images.size());
	static const bool _ = [&]{
	if (!vktexture::createlayout(objs))
		return false;
		return true;
	}();
	
	if (!vktexture::loadtexture(objs, mgltfobjs.texs, mmodel2))
		return false;
	if (!vktexture::loadtexset(objs, mgltfobjs.texs, *rvk::texlayout, mgltfobjs.dset, mmodel2))
		return false;

	createvboebo(objs);

	if (mmodel2.skins.size()) {
		getjointdata();
		getinvbindmats();

		mjnodecount = mmodel2.nodes.size();

		getanims();
	}else{
		skinned=false;
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
	const fastgltf::BufferView &bufferView = mmodel2.bufferViews.at(accessor.bufferViewIndex.value());
	const fastgltf::Buffer &buffer = mmodel2.buffers.at(bufferView.bufferIndex);

	minversebindmats.reserve(skin.joints.size());
	minversebindmats.resize(skin.joints.size());

	size_t newbytelength{skin.joints.size() * 4 * 4 * 4};
	std::visit(fastgltf::visitor{[](auto &arg) {},
	[&](const fastgltf::sources::Array &vector) {
		std::memcpy(minversebindmats.data(),
		            vector.bytes.data() + bufferView.byteOffset + accessor.byteOffset,
		            newbytelength);
	}},
	buffer.data);
}

void genericmodel::getanims() {
	manimclips.reserve(mmodel2.animations.size());
	for (auto &anim0 : mmodel2.animations) {
		std::shared_ptr<vkclip> clip0 = std::make_shared<vkclip>(static_cast<std::string>(anim0.name));
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
	std::visit(fastgltf::visitor{[](auto &arg) {},
	[&](fastgltf::TRS trs) {
		treeNode->settranslation(glm::make_vec3(trs.translation.data()));
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

std::vector<std::shared_ptr<vknode>> genericmodel::getnodelist(std::vector<std::shared_ptr<vknode>> &nodeList,
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

void genericmodel::createvboebo(rvk &objs) { //& joint vector

	jointuintofx.reserve(mmodel2.meshes.size());
	jointuintofx.resize(mmodel2.meshes.size());

	mgltfobjs.vbos.reserve(mmodel2.meshes.size());
	mgltfobjs.vbos.resize(mmodel2.meshes.size());
	mgltfobjs.ebos.reserve(mmodel2.meshes.size());
	mgltfobjs.ebos.resize(mmodel2.meshes.size());
	meshjointtype.reserve(mmodel2.meshes.size());
	meshjointtype.resize(mmodel2.meshes.size());
	for (size_t i{0}; i < mmodel2.meshes.size(); i++) {
		mgltfobjs.ebos[i].reserve(mmodel2.meshes[i].primitives.size());
		mgltfobjs.ebos[i].resize(mmodel2.meshes[i].primitives.size());
		mgltfobjs.vbos[i].reserve(mmodel2.meshes[i].primitives.size());
		mgltfobjs.vbos[i].resize(mmodel2.meshes[i].primitives.size());
		for (auto it = mmodel2.meshes[i].primitives.begin(); it < mmodel2.meshes[i].primitives.end(); it++) {

			if (it->type != fastgltf::PrimitiveType::Triangles)
				continue; // only drawing triangles

			const auto &idx = std::distance(mmodel2.meshes[i].primitives.begin(), it);
			mgltfobjs.vbos.at(i).at(idx).reserve(it->attributes.size());
			mgltfobjs.vbos.at(i).at(idx).resize(it->attributes.size());
			// it->
			const fastgltf::Accessor &idxacc = mmodel2.accessors[it->indicesAccessor.value()];
			vkebo::init(objs, mgltfobjs.ebos.at(i).at(idx),
			            idxacc.count * fastgltf::getComponentByteSize(idxacc.componentType));

			for (auto it2 = it->attributes.begin(); it2 < it->attributes.end(); it2++) {
				const auto &idx2 = std::distance(it->attributes.begin(), it2);
				// temp
				//  if(it2->name=="TEXCOORD_1"){
				//      mgltfobjs.vbodata.at(i).at(idx).erase(mgltfobjs.vbodata.at(i).at(idx).begin()+idx2);
				//      continue;
				//  }
				const fastgltf::Accessor &acc = mmodel2.accessors[it2->accessorIndex];

				// idfk how not to have it this way;
				if (it2->name == "POSITION") {

					vkvbo::init(objs, mgltfobjs.vbos.at(i).at(idx).at(0),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));
				} else if (it2->name == "NORMAL") {
					vkvbo::init(objs, mgltfobjs.vbos.at(i).at(idx).at(1),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));

				} else if (it2->name == "TEXCOORD_0") {
					vkvbo::init(objs, mgltfobjs.vbos.at(i).at(idx).at(2),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));

				} else if (it2->name == "JOINTS_0") {
					if (acc.componentType != fastgltf::ComponentType::UnsignedByte)
						vkvbo::init(objs, mgltfobjs.vbos.at(i).at(idx).at(3),
						            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType) * 4);
					else
						vkvbo::init(objs, mgltfobjs.vbos.at(i).at(idx).at(3),
						            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));

				} else if (it2->name == "WEIGHTS_0") {
					vkvbo::init(objs, mgltfobjs.vbos.at(i).at(idx).at(4),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));
				}
			}

			if (mmodel2.skins.size()) {
				const fastgltf::Accessor &joiacc = mmodel2.accessors[it->findAttribute("JOINTS_0")->accessorIndex];
				meshjointtype.at(i) = joiacc.componentType != fastgltf::ComponentType::UnsignedByte;

				const auto &joibview = mmodel2.bufferViews.at(joiacc.bufferViewIndex.value());
				const fastgltf::Buffer &buffer = mmodel2.buffers.at(joibview.bufferIndex); // important on demand

				if (joiacc.componentType == fastgltf::ComponentType::UnsignedShort) {
					if (i > 0)
						jointuintofx.at(i) = jointzint.size();
					else
						jointuintofx.at(i) = 0;
					jointz.reserve(joiacc.count * numotypes.at(joiacc.type));
					jointz.resize(joiacc.count * numotypes.at(joiacc.type));
					std::visit(fastgltf::visitor{[](auto &arg) {},
					[&](const fastgltf::sources::Array &vector) {
						std::memcpy(jointz.data(),
						            vector.bytes.data() + joibview.byteOffset + joiacc.byteOffset,
						            joibview.byteLength);
					}},
					buffer.data);
					jointzint.insert(jointzint.end(), jointz.begin(), jointz.end());
					jointz.clear();
				} else {
					if (i > 0)
						jointuintofx.at(i) = jointuintofx.at(i - 1);
					else
						jointuintofx.at(i) = 0;
				}
			}

			mgltfobjs.vbos.at(i).at(idx).shrink_to_fit(); // useless cause resize
		}
	}
}

void genericmodel::uploadvboebo(rvk &objs, VkCommandBuffer &cbuffer) {
	for (size_t i{0}; i < mmodel2.meshes.size(); i++) {
		for (auto it = mmodel2.meshes[i].primitives.begin(); it < mmodel2.meshes[i].primitives.end(); it++) {

			const fastgltf::Buffer &b = mmodel2.buffers[0];

			const auto &idx = std::distance(mmodel2.meshes[i].primitives.begin(), it);
			const fastgltf::Accessor &idxacc = mmodel2.accessors[it->indicesAccessor.value()];

			// const fastgltf::Accessor& posacc = mmodel2.accessors[it->findAttribute("POSITION")->accessorIndex];
			// if(!posacc.bufferViewIndex.has_value())continue; //gltf standard -> every primitive's verts must have position;

			const fastgltf::BufferView &idxbview = mmodel2.bufferViews[idxacc.bufferViewIndex.value()];
			vkebo::upload(objs, cbuffer, mgltfobjs.ebos.at(i).at(idx), b, idxbview, idxacc.count);

			// wip
			for (auto it2 = it->attributes.begin(); it2 < it->attributes.end(); it2++) {
				const auto &idx2 = std::distance(it->attributes.begin(), it2);
				const fastgltf::Accessor &acc = mmodel2.accessors[it2->accessorIndex];
				const fastgltf::BufferView &bview = mmodel2.bufferViews[acc.bufferViewIndex.value()];
				if (it2->name.starts_with("JOINTS_")) {
					if (it2->name == "JOINTS_0")
						if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
							vkvbo::upload(objs, cbuffer, mgltfobjs.vbos.at(i).at(idx).at(3), jointzint, acc.count,
							              jointuintofx[i]);
						} else {
							vkvbo::upload(objs, cbuffer, mgltfobjs.vbos.at(i).at(idx).at(3), b, bview, acc);
						}
				} else if (it2->name.starts_with("TEXCOORD_")) {
					if (it2->name == "TEXCOORD_0") // only 1 cause shader not ready
						vkvbo::upload(objs, cbuffer, mgltfobjs.vbos.at(i).at(idx).at(2), b, bview, acc);
				} else if (it2->name.starts_with("WEIGHTS_")) {
					if (it2->name == "WEIGHTS_0")
						vkvbo::upload(objs, cbuffer, mgltfobjs.vbos.at(i).at(idx).at(4), b, bview, acc);
				} else if (it2->name.starts_with("NORMAL")) {
					vkvbo::upload(objs, cbuffer, mgltfobjs.vbos.at(i).at(idx).at(1), b, bview, acc);
				} else if (it2->name.starts_with("POSITION")) {
					vkvbo::upload(objs, cbuffer, mgltfobjs.vbos.at(i).at(idx).at(0), b, bview, acc);
				} else {
					std::cout << "new attribute : " << it2->name << " : ignored" << std::endl;
				}
			}
		}
	}
}

size_t genericmodel::gettricount(size_t i, size_t j) {
	const fastgltf::Primitive &prims = mmodel2.meshes.at(i).primitives.at(j);
	const fastgltf::Accessor &acc = mmodel2.accessors.at(prims.indicesAccessor.value());
	size_t c{acc.count / 3};
	return c;
}

void genericmodel::drawinstanced(rvk &objs, VkPipelineLayout &vkplayout, VkPipeline &vkpline,
                                 VkPipeline &vkplineuint, int instancecount, int stride) {
	VkDeviceSize offset = 0;
	std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.vbos.size());

	vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(rvk::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1,
	                        &mgltfobjs.dset, 0, nullptr);

	for (size_t i{0}; i < mgltfobjs.vbos.size(); i++) {
		pushes[i].reserve(mgltfobjs.vbos.at(i).size());
		pushes[i].resize(mgltfobjs.vbos.at(i).size());

		meshjointtype[i] ? vkCmdBindPipeline(objs.cbuffers_graphics.at(rvk::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, vkplineuint)
		: vkCmdBindPipeline(objs.cbuffers_graphics.at(rvk::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, vkpline);

		for (size_t j{0}; j < mgltfobjs.vbos.at(i).size(); j++) {
			pushes[i][j].stride = stride;
			if (mmodel2.meshes.at(i).primitives.at(j).materialIndex.has_value() &&
			        mmodel2.materials.at(mmodel2.meshes.at(i).primitives.at(j).materialIndex.value())
			        .pbrData.baseColorTexture.has_value()) {
				pushes[i][j].texidx = static_cast<unsigned int>(
				                          mmodel2
				                          .textures[mmodel2.materials[mmodel2.meshes.at(i).primitives.at(j).materialIndex.value_or(0)]
				                                    .pbrData.baseColorTexture->textureIndex]
				                          .imageIndex.value_or(0));
			} else {
				pushes[i][j].texidx = 0;
			}
			pushes[i][j].t = static_cast<float>(SDL_GetTicks()) / 1000.0f;

			vkCmdPushConstants(objs.cbuffers_graphics.at(rvk::currentFrame), vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants),
			                   &pushes.at(i).at(j));
			// rework bindings
			for (size_t k{0}; k < mgltfobjs.vbos.at(i).at(j).size(); k++) {
				if (mgltfobjs.vbos.at(i).at(j).at(k).buffer != VK_NULL_HANDLE)
					vkCmdBindVertexBuffers(objs.cbuffers_graphics.at(rvk::currentFrame), k, 1, &mgltfobjs.vbos.at(i).at(j).at(k).buffer,
					                       &offset);
			}
			vkCmdBindIndexBuffer(objs.cbuffers_graphics.at(rvk::currentFrame), mgltfobjs.ebos.at(i).at(j).buffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(objs.cbuffers_graphics.at(rvk::currentFrame), static_cast<uint32_t>(gettricount(i, j) * 3), instancecount, 0, 0, 0);
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


void genericmodel::drawinstancedstatic(rvk &objs, VkPipelineLayout &vkplayout, VkPipeline &vkpline,
                                  int instancecount, int stride) {
	VkDeviceSize offset = 0;
	std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.vbos.size());

	vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(rvk::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1,
	                        &mgltfobjs.dset, 0, nullptr);

	for (size_t i{0}; i < mgltfobjs.vbos.size(); i++) {
		pushes[i].reserve(mgltfobjs.vbos.at(i).size());
		pushes[i].resize(mgltfobjs.vbos.at(i).size());
		
		vkCmdBindPipeline(objs.cbuffers_graphics.at(rvk::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, vkpline);

		for (size_t j{0}; j < mgltfobjs.vbos.at(i).size(); j++) {
			pushes[i][j].stride = stride;
			if (mmodel2.meshes.at(i).primitives.at(j).materialIndex.has_value() &&
			        mmodel2.materials.at(mmodel2.meshes.at(i).primitives.at(j).materialIndex.value())
			        .pbrData.baseColorTexture.has_value()) {
				pushes[i][j].texidx = static_cast<unsigned int>(
				                          mmodel2
				                          .textures[mmodel2.materials[mmodel2.meshes.at(i).primitives.at(j).materialIndex.value_or(0)]
				                                    .pbrData.baseColorTexture->textureIndex]
				                          .imageIndex.value_or(0));
			} else {
				pushes[i][j].texidx = 0;
			}
			pushes[i][j].t = static_cast<float>(SDL_GetTicks()) / 1000.0f;

			vkCmdPushConstants(objs.cbuffers_graphics.at(rvk::currentFrame), vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants),
			                   &pushes.at(i).at(j));
			// rework bindings
			for (size_t k{0}; k < mgltfobjs.vbos.at(i).at(j).size(); k++) {
				if (mgltfobjs.vbos.at(i).at(j).at(k).buffer != VK_NULL_HANDLE)
					vkCmdBindVertexBuffers(objs.cbuffers_graphics.at(rvk::currentFrame), k, 1, &mgltfobjs.vbos.at(i).at(j).at(k).buffer,
					                       &offset);
			}
			vkCmdBindIndexBuffer(objs.cbuffers_graphics.at(rvk::currentFrame), mgltfobjs.ebos.at(i).at(j).buffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(objs.cbuffers_graphics.at(rvk::currentFrame), static_cast<uint32_t>(gettricount(i, j) * 3), instancecount, 0, 0, 0);
		}
	}
}

std::vector<texdata> genericmodel::gettexdata() {
	return mgltfobjs.texs;
}
