
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

bool genericmodel::loadmodel(vkobjs &objs, std::string fname) {

	fastgltf::Parser fastparser{};
	auto buff = fastgltf::MappedGltfFile::FromPath(fname);
	auto a = fastparser.loadGltfBinary(buff.get(), std::filesystem::absolute(fname));
	mmodel2 = std::move(a.get());

	mgltfobjs.tex.reserve(mmodel2.images.size());
	mgltfobjs.tex.resize(mmodel2.images.size());
	if (!vktexture::loadtexture(objs, mgltfobjs.tex, mmodel2))
		return false;
	if (!vktexture::loadtexlayoutpool(objs, mgltfobjs.tex, mgltfobjs.texpls, mmodel2))
		return false;

	createvboebo(objs);

	if (mmodel2.skins.size()) {
		getjointdata();
		getinvbindmats();

		mjnodecount = mmodel2.nodes.size();

		getanims();
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
	// for (auto& anim0 : mmodel->animations) {
	//     std::shared_ptr<vkclip> clip0=std::make_shared<vkclip>(anim0.name);
	//     for (auto& c : anim0.channels) {
	//         clip0->addchan(mmodel, anim0, c);
	//     }
	//     manimclips.push_back(clip0);
	// }
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

	// auto removeIt = std::remove_if(childNodes.begin(), childNodes.end(),
	//     [&](size_t& num) { return mmodel2.nodes.at(num).skinIndex != -1; }
	//);
	// childNodes.erase(removeIt, childNodes.end());

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

void genericmodel::createvboebo(vkobjs &objs) { //& joint vector

	jointuintofx.reserve(mmodel2.meshes.size());
	jointuintofx.resize(mmodel2.meshes.size());

	mgltfobjs.vbodata.reserve(mmodel2.meshes.size());
	mgltfobjs.vbodata.resize(mmodel2.meshes.size());
	mgltfobjs.ebodata.reserve(mmodel2.meshes.size());
	mgltfobjs.ebodata.resize(mmodel2.meshes.size());
	meshjointtype.reserve(mmodel2.meshes.size());
	meshjointtype.resize(mmodel2.meshes.size());
	for (size_t i{0}; i < mmodel2.meshes.size(); i++) {
		mgltfobjs.ebodata[i].reserve(mmodel2.meshes[i].primitives.size());
		mgltfobjs.ebodata[i].resize(mmodel2.meshes[i].primitives.size());
		mgltfobjs.vbodata[i].reserve(mmodel2.meshes[i].primitives.size());
		mgltfobjs.vbodata[i].resize(mmodel2.meshes[i].primitives.size());
		for (auto it = mmodel2.meshes[i].primitives.begin(); it < mmodel2.meshes[i].primitives.end(); it++) {

			if (it->type != fastgltf::PrimitiveType::Triangles)
				continue; // only drawing triangles

			const auto &idx = std::distance(mmodel2.meshes[i].primitives.begin(), it);
			mgltfobjs.vbodata.at(i).at(idx).reserve(it->attributes.size());
			mgltfobjs.vbodata.at(i).at(idx).resize(it->attributes.size());
			// it->
			const fastgltf::Accessor &idxacc = mmodel2.accessors[it->indicesAccessor.value()];
			vkebo::init(objs, mgltfobjs.ebodata.at(i).at(idx),
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

					vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(0),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));
				} else if (it2->name == "NORMAL") {
					vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(1),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));

				} else if (it2->name == "TEXCOORD_0") {
					vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(2),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));

				} else if (it2->name == "JOINTS_0") {
					if (acc.componentType != fastgltf::ComponentType::UnsignedByte)
						vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(3),
						            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType) * 4);
					else
						vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(3),
						            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));

				} else if (it2->name == "WEIGHTS_0") {
					vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(4),
					            acc.count * fastgltf::getElementByteSize(acc.type, acc.componentType));
				}
			}

			// const fastgltf::Accessor& posacc = mmodel2.accessors[it->findAttribute("POSITION")->accessorIndex];
			// if(!posacc.bufferViewIndex.has_value())continue; //gltf standard -> every primitive's verts must have position;

			// vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(0), posacc.count *
			// fastgltf::getElementByteSize(posacc.type,posacc.componentType));

			// const fastgltf::Accessor& noracc = mmodel2.accessors[it->findAttribute("NORMAL")->accessorIndex];

			// if(it->materialIndex.has_value()){
			//     if(mmodel2.materials.at(it->materialIndex.value()).pbrData.baseColorTexture.has_value()){
			//         const auto& texidx =
			//         mmodel2.materials.at(it->materialIndex.value()).pbrData.baseColorTexture->texCoordIndex;
			//         // while(const auto& tcoord{it->findAttribute("TEXCOORD_" + std::to_string(texidx))->accessorIndex}!=);
			//         const fastgltf::Accessor& texacc = mmodel2.accessors[it->findAttribute("TEXCOORD_" +
			//         std::to_string(texidx))->accessorIndex]; if(&texacc!=&posacc) vkvbo::init(objs,
			//         mgltfobjs.vbodata.at(i).at(idx).at(2), texacc.count *
			//         fastgltf::getElementByteSize(texacc.type,texacc.componentType));
			//     }
			// }
			// const fastgltf::Accessor& joiacc = mmodel2.accessors[it->findAttribute("JOINTS_0")->accessorIndex];
			// const fastgltf::Accessor& weiacc = mmodel2.accessors[it->findAttribute("WEIGHTS_0")->accessorIndex];
			// const fastgltf::Accessor& noacc = mmodel2.accessors[it->findAttribute("nothing")->accessorIndex];
			// if(&noracc!=&noacc)
			// vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(1), noracc.count *
			// fastgltf::getElementByteSize(noracc.type,noracc.componentType));
			// // if(joiacc){  //todo
			// // if(weiacc){  //todo
			// // std::cout << joiacc.componentType << std::endl;
			// // if(joiacc.)
			// // if()
			// if(&joiacc!=&noacc){
			// vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(3), joiacc.count *
			// fastgltf::getElementByteSize(joiacc.type,joiacc.componentType));

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

			// std::vector<fastgltf::ComponentType>
			// byters{fastgltf::ComponentType::Byte,fastgltf::ComponentType::UnsignedByte};

			// meshjointtype.at(i) = std::any_of(byters.begin(),byters.end(),[&joiacc](fastgltf::ComponentType x){
			//     return joiacc.componentType == x;
			// });

			// if(&weiacc!=&noacc)
			// vkvbo::init(objs, mgltfobjs.vbodata.at(i).at(idx).at(4), weiacc.count *
			// fastgltf::getElementByteSize(weiacc.type,weiacc.componentType));

			mgltfobjs.vbodata.at(i).at(idx).shrink_to_fit(); // useless cause resize
		}
	}
}

void genericmodel::uploadvboebo(vkobjs &objs, VkCommandBuffer &cbuffer) {
	for (size_t i{0}; i < mmodel2.meshes.size(); i++) {
		for (auto it = mmodel2.meshes[i].primitives.begin(); it < mmodel2.meshes[i].primitives.end(); it++) {

			const fastgltf::Buffer &b = mmodel2.buffers[0];

			const auto &idx = std::distance(mmodel2.meshes[i].primitives.begin(), it);
			const fastgltf::Accessor &idxacc = mmodel2.accessors[it->indicesAccessor.value()];

			// const fastgltf::Accessor& posacc = mmodel2.accessors[it->findAttribute("POSITION")->accessorIndex];
			// if(!posacc.bufferViewIndex.has_value())continue; //gltf standard -> every primitive's verts must have position;

			const fastgltf::BufferView &idxbview = mmodel2.bufferViews[idxacc.bufferViewIndex.value()];
			vkebo::upload(objs, cbuffer, mgltfobjs.ebodata.at(i).at(idx), b, idxbview, idxacc.count);

			// wip
			for (auto it2 = it->attributes.begin(); it2 < it->attributes.end(); it2++) {
				const auto &idx2 = std::distance(it->attributes.begin(), it2);
				const fastgltf::Accessor &acc = mmodel2.accessors[it2->accessorIndex];
				const fastgltf::BufferView &bview = mmodel2.bufferViews[acc.bufferViewIndex.value()];
				if (it2->name.starts_with("JOINTS_")) {
					if (it2->name == "JOINTS_0")
						if (acc.componentType == fastgltf::ComponentType::UnsignedShort) {
							vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(3), jointzint, acc.count,
							              jointuintofx[i]);
						} else {
							vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(3), b, bview, acc);
						}
				} else if (it2->name.starts_with("TEXCOORD_")) {
					if (it2->name == "TEXCOORD_0") // only 1 cause shader not ready
						vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(2), b, bview, acc);
				} else if (it2->name.starts_with("WEIGHTS_")) {
					if (it2->name == "WEIGHTS_0")
						vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(4), b, bview, acc);
				} else if (it2->name.starts_with("NORMAL")) {
					vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(1), b, bview, acc);
				} else if (it2->name.starts_with("POSITION")) {
					vkvbo::upload(objs, cbuffer, mgltfobjs.vbodata.at(i).at(idx).at(0), b, bview, acc);
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

void genericmodel::drawinstanced(vkobjs &objs, VkPipelineLayout &vkplayout, VkPipeline &vkpline,
                                 VkPipeline &vkplineuint, int instancecount, int stride) {
	VkDeviceSize offset = 0;
	std::vector<std::vector<vkpushconstants>> pushes(mgltfobjs.vbodata.size());

	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplayout, 0, 1,
	                        &mgltfobjs.texpls.texdescriptorset, 0, nullptr);

	for (int i{0}; i < mgltfobjs.vbodata.size(); i++) {
		pushes[i].reserve(mgltfobjs.vbodata.at(i).size());
		pushes[i].resize(mgltfobjs.vbodata.at(i).size());

		meshjointtype[i] ? vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkplineuint)
		: vkCmdBindPipeline(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, vkpline);

		for (int j{0}; j < mgltfobjs.vbodata.at(i).size(); j++) {
			pushes[i][j].pkmodelstride = stride;
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
			pushes[i][j].decaying = false;

			vkCmdPushConstants(objs.rdcommandbuffer[0], vkplayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vkpushconstants),
			                   &pushes.at(i).at(j));
			// rework bindings
			for (int k{0}; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
				if (mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer != VK_NULL_HANDLE)
					vkCmdBindVertexBuffers(objs.rdcommandbuffer[0], k, 1, &mgltfobjs.vbodata.at(i).at(j).at(k).rdvertexbuffer,
					                       &offset);
			}
			vkCmdBindIndexBuffer(objs.rdcommandbuffer[0], mgltfobjs.ebodata.at(i).at(j).bhandle, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(objs.rdcommandbuffer[0], static_cast<uint32_t>(gettricount(i, j) * 3), instancecount, 0, 0, 0);
		}
	}
}

void genericmodel::cleanup(vkobjs &objs) {

	for (int i{0}; i < mgltfobjs.vbodata.size(); i++) {
		for (int j{0}; j < mgltfobjs.vbodata.at(i).size(); j++) {
			for (int k{0}; k < mgltfobjs.vbodata.at(i).at(j).size(); k++) {
				vkvbo::cleanup(objs, mgltfobjs.vbodata.at(i).at(j).at(k));
			}
		}
	}
	for (int i{0}; i < mgltfobjs.ebodata.size(); i++) {
		for (int j{0}; j < mgltfobjs.ebodata.at(i).size(); j++) {
			vkebo::cleanup(objs, mgltfobjs.ebodata.at(i).at(j));
		}
	}
	for (int i{0}; i < mgltfobjs.tex.size(); i++) {
		vktexture::cleanup(objs, mgltfobjs.tex[i]);
	}
	vktexture::cleanuppls(objs, mgltfobjs.texpls);

	// mmodel.reset();
}

std::vector<vktexdata> genericmodel::gettexdata() {
	return mgltfobjs.tex;
}

vktexdatapls genericmodel::gettexdatapls() {
	return mgltfobjs.texpls;
}
