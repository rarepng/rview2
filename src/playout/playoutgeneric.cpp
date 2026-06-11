#include "playoutgeneric.hpp"
#include "buffer/ssbo.hpp"
#include "core/rvk.hpp"
#include "modelsettings.hpp"
#include "playout.hpp"
#include "pline.hpp"
#include "ubo.hpp"
#include "vktex.hpp"
#include <iostream>

bool playoutgeneric::setup(rvkbucket &objs, std::string_view fname, size_t count, std::string_view vfile, std::string_view ffile) {
	static const bool _ = [&] {
		if (!createpline(objs, vfile, ffile))
			return false;

		return true;
	}();

	if (!loadmodel(objs, fname))
		return false;

	m_modelID = mgltf->assigned_model_id;

	if (!createinstances(objs, count, false))
		return false;

	if (mgltf->skinned) {
		if (!createssbomat(objs))
			return false;
	} else {
		if (!createssbostatic(objs))
			return false;
	}

	ready = true;
	return _;
}
void playoutgeneric::sync_scene_data() {
	for (const auto& inst : minstances) {
		if (inst->getinstancesettings().msdrawmodel) {
			inst->sync_to_scene(m_modelID);
		}
	}
}
bool playoutgeneric::loadmodel(rvkbucket &objs, std::string_view fname) {
	mgltf = std::make_shared<genericmodel>();

	if (!mgltf->loadmodel(objs, fname))
		return false;

	return true;
}
size_t playoutgeneric::instcount() {
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
bool playoutgeneric::createpline(rvkbucket &objs, std::string_view vfile, std::string_view ffile) {
	if (!pline::init(objs, rview::core::globalPipelineLayout, skinnedpline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	                 std::vector<std::string_view> {vfile, ffile}, objs.schain.image_format, objs.rddepthformat))
		return false;

	if (!pline::init(objs, rview::core::globalPipelineLayout, skinnedplineuint, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	                 std::vector<std::string_view> {vfile, ffile}, objs.schain.image_format, objs.rddepthformat))
		return false;

	if (!pline::initcompute(objs, rview::core::globalPipelineLayout, rview::core::globalcullpline, std::vector<std::string_view> {"shaders/cx.spv"}))
		return false;
	return true;
}

void playoutgeneric::updateanims() {
	if (mgltf->skinned)
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

void playoutgeneric::uploadubossbo(rvkbucket &objs, std::vector<glm::mat4>& cammats, const glm::vec3& campos) {
	if (mgltf->skinned) {
		ssbo::upload(objs, rdjointmatrixssbo, jointmats.data(), jointmats.size());
	} else {
		glm::mat4 identity(1.0f);
		ssbo::upload(objs, rdjointmatrixssbo, &identity, 1);
	}

	ssbo::upload(objs, rdjointmatrixssbo, jointmats);

}

std::shared_ptr<genericinstance> playoutgeneric::getinst(int i) {
	return minstances[i];
}

void playoutgeneric::updatemats() {
	jointmats.clear();
	jointdqs.clear();
	numdqs = 0;
	nummats = 0;

	if (mgltf->skinned) {
		uint32_t currentOffset = 0;

		for (const auto &i : minstances) {
			modelsettings &settings = i->getinstancesettings();

			if (!settings.msdrawmodel) continue;

			i->set_joint_offset(currentOffset);

			if (settings.mvertexskinningmode == skinningmode::dualquat) {
				std::vector<glm::mat2x4> quats = i->getjointdualquats();
				jointdqs.insert(jointdqs.end(), quats.begin(), quats.end());
				numdqs++;
			} else {
				std::vector<glm::mat4> mats = i->getjointmats();
				jointmats.insert(jointmats.end(), mats.begin(), mats.end());
				currentOffset += mats.size();
				nummats++;
			}
		}
	} else {
		for (const auto &i : minstances) {
			modelsettings &settings = i->getinstancesettings();

			if (!settings.msdrawmodel) continue;

			i->set_joint_offset(0);
			jointmats.push_back(i->calcstaticmat());
			nummats++;
		}
	}
}

void playoutgeneric::cleanuplines(rvkbucket &objs) {
	static const bool _ = [&] {
		pline::cleanup(objs, skinnedpline);
		pline::cleanup(objs, skinnedplineuint);
		pline::cleanup(objs, rview::core::globalcullpline);
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

void playoutgeneric::draw(rvkbucket &objs, uint32_t indirectoffset) {
	if (minstances[0]->getinstancesettings().msdrawmodel) {

		stride = mgltf->skinned ? minstances.at(0)->getjointmatrixsize() : 1;

		mgltf->drawinstanced(objs, rview::core::globalPipelineLayout, skinnedpline, skinnedplineuint, numinstancess, stride, m_modelID, indirectoffset);
	}
}
