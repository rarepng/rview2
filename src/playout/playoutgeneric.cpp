#include "playoutgeneric.hpp"
#include "buffer/ssbo.hpp"
#include "core/rvk.hpp"
#include "modelsettings.hpp"
#include "playout.hpp"
#include "pline.hpp"
#include "ubo.hpp"
#include "vktex.hpp"
#include <iostream>

bool playoutgeneric::setup(rvkbucket &objs, std::string fname, size_t count, std::string vfile, std::string ffile) {
		m_modelID = rvkbucket::globalModelCounter.fetch_add(1, std::memory_order_relaxed);
		static const bool _ = [&] {
		// if (!rview::rvk::tex::createlayout(objs, rvkbucket::texlayout)) return false;
		// if (!ubo::createlayout(objs, rvkbucket::ubolayout)) return false;
		// if (!ssbo::createlayout(objs, rvkbucket::ssbolayout)) return false;
		// if(!ssbo::createmateriallayout(objs, rvkbucket::materiallayout))
		// 	return false;
		// for (uint32_t i = 0; i < rvkbucket::MAX_GLOBAL_MATERIALS; ++i) {
		// 	rvkbucket::global_materials.free_slots.push(i);
		// }

		// 2. Self-Healing Dedicated Descriptor Pool
		// VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
		// VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		// poolInfo.poolSizeCount = 1;
		// poolInfo.pPoolSizes = &poolSize;
		// poolInfo.maxSets = 1;
		// vkCreateDescriptorPool(objs.vkdevice.device, &poolInfo, nullptr, &rvkbucket::global_materials.dedicated_pool);

		// // 3. Allocate Massive SSBO with PERSISTENT MAPPING
		// size_t bufferSize = rvkbucket::MAX_GLOBAL_MATERIALS * sizeof(MaterialData);
		// VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		// binfo.size = bufferSize;
		// binfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		
		// VmaAllocationCreateInfo vmaallocinfo{};
		// vmaallocinfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		// vmaallocinfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // forever mapped!!!!!!!!!! keep that way
		
		// VmaAllocationInfo allocResult;
		// if (vmaCreateBuffer(objs.alloc, &binfo, &vmaallocinfo, 
		//     &rvkbucket::global_materials.buffer, 
		//     &rvkbucket::global_materials.alloc, &allocResult) != VK_SUCCESS) return false;
		
		// rvkbucket::global_materials.mapped_data = allocResult.pMappedData;

		// // 4. Allocate and Update Set
		// VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		// allocInfo.descriptorPool = rvkbucket::global_materials.dedicated_pool;
		// allocInfo.descriptorSetCount = 1;
		// allocInfo.pSetLayouts = &rvkbucket::materiallayout;
		// vkAllocateDescriptorSets(objs.vkdevice.device, &allocInfo, &rvkbucket::global_materials.dset);

		// VkDescriptorBufferInfo ssboInfo{ rvkbucket::global_materials.buffer, 0, bufferSize };
		// VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		// write.dstSet = rvkbucket::global_materials.dset;
		// write.dstBinding = 0;
		// write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		// write.descriptorCount = 1;
		// write.pBufferInfo = &ssboInfo;
		// vkUpdateDescriptorSets(objs.vkdevice.device, 1, &write, 0, nullptr);
		
		// if (!createubo(objs))
		// 	return false;
		// if (!createskinnedplayout(objs))
		// 	return false;
		if (!createpline(objs, vfile, ffile))
			return false;
		return true;
	}();

	if (!loadmodel(objs, fname))
		return false;
	if (!createinstances(objs, count, false))
		return false;

	if (mgltf->skinned) {
		if (!createssbomat(objs))
			return false;
	} else {
		if(!createssbostatic(objs))
			return false;
	}

	ready = true;
	return _;
}
bool playoutgeneric::loadmodel(rvkbucket &objs, std::string fname) {
	mgltf = std::make_shared<genericmodel>();
	if (!mgltf->loadmodel(objs, fname))
		return false;
	return true;
}
size_t playoutgeneric::instcount(){
	return numinstancess;
}
bool playoutgeneric::createinstances(rvkbucket &objs, size_t count, bool rand) {
	size_t numTriangles{};
	for (size_t i{0}; i < count; ++i) {
		minstances.emplace_back(std::make_shared<genericinstance>(mgltf, glm::vec3{0.0f, 0.0f, 0.0f}, rand));
		numTriangles += mgltf->gettricount(0, 0);
	}
	totaltricount = numTriangles;
	numinstancess = count;

	if (!minstances.size())
		return false;
	return true;
}
// bool playoutgeneric::createubo(rvkbucket &objs) {
// 	if (!ubo::init(objs, rdperspviewmatrixubo))
// 		return false;
// 	return true;
// }

bool playoutgeneric::createssbomat(rvkbucket &objs) {
    size_t size = numinstancess * SDL_clamp(minstances[0]->getjointmatrixsize(), 1, minstances[0]->getjointmatrixsize()) * sizeof(glm::mat4);
    if (!ssbo::init_bindless(objs, rdjointmatrixssbo, size, m_modelID))
        return false;
    return true;
}

bool playoutgeneric::createssbostatic(rvkbucket &objs) {
    size_t size = numinstancess * sizeof(glm::mat4);
    if (!ssbo::init_bindless(objs, rdjointmatrixssbo, size, m_modelID))
        return false;
    return true;
}
bool playoutgeneric::createpline(rvkbucket &objs, std::string vfile, std::string ffile) {
	if (!pline::init(objs, rvkbucket::globalPipelineLayout, skinnedpline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 6, 63,
	                 std::vector<std::string> {vfile, ffile},false,objs.schain.image_format,objs.rddepthformat))
		return false;
	if (!pline::init(objs, rvkbucket::globalPipelineLayout, skinnedplineuint, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 6, 63,
	                 std::vector<std::string> {vfile, ffile}, true,objs.schain.image_format,objs.rddepthformat))
		return false;
	return true;
}

void playoutgeneric::updateanims() {
	if(mgltf->skinned)
		for (auto &i : minstances) {
			i->updateanimation();
			i->solveik();
		}
}

void playoutgeneric::uploadvboebo(rvkbucket &objs, VkCommandBuffer &cbuffer) {
	if (uploadreq) {
		mgltf->uploadvboebo(objs, cbuffer);
		uploadreq = false;
	}
}

void playoutgeneric::uploadubossbo(rvkbucket &objs, std::vector<glm::mat4> &cammats,const glm::vec3& campos) {
	// vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedplayout, 1, 1,
	//                         &rdperspviewmatrixubo[0].dset, 0, nullptr);
	// ubo::upload(objs, rdperspviewmatrixubo, cammats,campos);
	// vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0).at(rvkbucket::currentFrame), VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedplayout, 2, 1,
	//                         &rdjointmatrixssbo.dset, 0, nullptr);
	ssbo::upload(objs, rdjointmatrixssbo, jointmats);

}

std::shared_ptr<genericinstance> playoutgeneric::getinst(int i) {
	return minstances[i];
}

void playoutgeneric::updatemats() {

	//might have to put if skinned here

	totaltricount = 0;
	jointmats.clear();
	jointdqs.clear();

	numdqs = 0;
	nummats = 0;
	if(mgltf->skinned) {

		for (const auto &i : minstances) {
			modelsettings &settings = i->getinstancesettings();
			if (!settings.msdrawmodel)
				continue;
			if (settings.mvertexskinningmode == skinningmode::dualquat) {
				std::vector<glm::mat2x4> quats = i->getjointdualquats();
				jointdqs.insert(jointdqs.end(), quats.begin(), quats.end());
				numdqs++;
			} else {
				std::vector<glm::mat4> mats = i->getjointmats();
				jointmats.insert(jointmats.end(), mats.begin(), mats.end());
				nummats++;
			}
			totaltricount += mgltf->gettricount(0, 0);
		}
	} else {
		//todo dual quats
		for (const auto &i : minstances) {
			modelsettings &settings = i->getinstancesettings();
			if (!settings.msdrawmodel)
				continue;
			jointmats.push_back(i->calcstaticmat());
			nummats++;
			totaltricount += mgltf->gettricount(0, 0);
		}
	}
}

void playoutgeneric::cleanuplines(rvkbucket &objs) {
	static const bool _ = [&] {
		pline::cleanup(objs, skinnedpline);
		pline::cleanup(objs, skinnedplineuint);
		return true;
	}();
}

void playoutgeneric::cleanupbuffers(rvkbucket &objs) {
	static const bool _ = [&] {
		// ubo::cleanup(objs, rdperspviewmatrixubo);
		return true;
	}();
	ssbo::cleanup(objs, rdjointmatrixssbo);
}

void playoutgeneric::cleanupmodels(rvkbucket &objs) {
	mgltf->cleanup(objs);
	mgltf.reset();
}

void playoutgeneric::draw(rvkbucket &objs) {
    if (minstances[0]->getinstancesettings().msdrawmodel) {
        
        stride = mgltf->skinned ? minstances.at(0)->getjointmatrixsize() : 1;
        
        mgltf->drawinstanced(objs, rvkbucket::globalPipelineLayout, skinnedpline, skinnedplineuint, numinstancess, stride, m_modelID);
    }
}
