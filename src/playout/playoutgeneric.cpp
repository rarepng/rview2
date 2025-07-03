#include "playoutgeneric.hpp"
#include "buffer/ssbo.hpp"
#include "modelsettings.hpp"
#include "playout.hpp"
#include "pline.hpp"
#include "ubo.hpp"
#include <iostream>

bool playoutgeneric::setup(rvk &objs, std::string fname, size_t count, std::string vfile, std::string ffile) {
	if (!loadmodel(objs, fname))
		return false;
	if (!createinstances(objs, count, false))
		return false;
	static const bool _ = [&] {
		if(!ssbo::createlayout(objs,rvk::ssbolayout))
			return false;
		if (!createubo(objs))
			return false;
		if (!createskinnedplayout(objs))
			return false;
		if (!createstaticplayout(objs))
			return false;
		if (!createplinestatic(objs))
			return false;
		if (!createpline(objs, vfile, ffile))
			return false;
		return true;
	}();

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

bool playoutgeneric::loadmodel(rvk &objs, std::string fname) {
	mgltf = std::make_shared<genericmodel>();
	if (!mgltf->loadmodel(objs, fname))
		return false;
	return true;
}

bool playoutgeneric::createinstances(rvk &objs, size_t count, bool rand) {
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
bool playoutgeneric::createubo(rvk &objs) {
	ubo::createlayout(objs,rvk::ubolayout);
	if (!ubo::init(objs, rdperspviewmatrixubo))
		return false;
	return true;
}

bool playoutgeneric::createssbomat(rvk &objs) {
	size_t size = numinstancess * SDL_clamp(minstances[0]->getjointmatrixsize(), 1, minstances[0]->getjointmatrixsize()) *
	              sizeof(glm::mat4);
	if (!ssbo::init(objs, rdjointmatrixssbo, size))
		return false;
	return true;
}
bool playoutgeneric::createssbostatic(rvk &objs) {
	size_t size = numinstancess * sizeof(glm::mat4);
	if (!ssbo::init(objs, rdjointmatrixssbo, size))
		return false;
	return true;
}
bool playoutgeneric::createskinnedplayout(rvk &objs) {
	std::array<VkDescriptorSetLayout,3> dlayouts{*rvk::texlayout,rvk::ubolayout,rvk::ssbolayout};
	if (!playout::init(objs, skinnedplayout, dlayouts, sizeof(vkpushconstants)))
		return false;
	return true;
}
bool playoutgeneric::createstaticplayout(rvk &objs) {
	std::array<VkDescriptorSetLayout,3> dlayouts{*rvk::texlayout,rvk::ubolayout,rvk::ssbolayout};
	if (!playout::init(objs, staticplayout, dlayouts, sizeof(vkpushconstants)))
		return false;
	return true;
}

bool playoutgeneric::createpline(rvk &objs, std::string vfile, std::string ffile) {
	if (!pline::init(objs, skinnedplayout, skinnedpline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 5, 31,
	                 std::vector<std::string> {vfile, ffile}))
		return false;
	if (!pline::init(objs, skinnedplayout, skinnedplineuint, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 5, 31,
	                 std::vector<std::string> {vfile, ffile}, true))
		return false;
	return true;
}
bool playoutgeneric::createplinestatic(rvk &objs) {
	if (!pline::init(objs, staticplayout, staticpline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 3, 7,
	                 std::vector<std::string> {"shaders/svx.spv", "shaders/spx.spv"}))
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

void playoutgeneric::uploadvboebo(rvk &objs, VkCommandBuffer &cbuffer) {
	if (uploadreq) {
		mgltf->uploadvboebo(objs, cbuffer);
		uploadreq = false;
	}
}

void playoutgeneric::uploadubossbo(rvk &objs, std::vector<glm::mat4> &cammats) {
	vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, mgltf->skinned ? skinnedplayout : staticplayout, 1, 1,
	                        &rdperspviewmatrixubo[0].dset, 0, nullptr);
	ubo::upload(objs, rdperspviewmatrixubo, cammats);
	if(mgltf->skinned) {
		vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedplayout, 2, 1,
		                        &rdjointmatrixssbo.dset, 0, nullptr);
		ssbo::upload(objs, rdjointmatrixssbo, jointmats);
	} else {
		vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, staticplayout, 2, 1,
		                        &rdjointmatrixssbo.dset, 0, nullptr);
		ssbo::upload(objs, rdjointmatrixssbo, jointmats);
	}
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

void playoutgeneric::cleanuplines(rvk &objs) {
	static const bool _ = [&] {
		pline::cleanup(objs, skinnedpline);
		pline::cleanup(objs, skinnedplineuint);
		pline::cleanup(objs,staticpline);
		playout::cleanup(objs, skinnedplayout);
		playout::cleanup(objs, staticplayout);
		return true;
	}();
}

void playoutgeneric::cleanupbuffers(rvk &objs) {
	static const bool _ = [&] {
		ubo::cleanup(objs, rdperspviewmatrixubo);
		return true;
	}();
		ssbo::cleanup(objs, rdjointmatrixssbo);
}

void playoutgeneric::cleanupmodels(rvk &objs) {
	mgltf->cleanup(objs);
	mgltf.reset();
}

void playoutgeneric::draw(rvk &objs) {
	if (minstances[0]->getinstancesettings().msdrawmodel) {

		if(mgltf->skinned) {
			stride = minstances.at(0)->getjointmatrixsize();

			vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedplayout, 1, 1,
			                        &rdperspviewmatrixubo[0].dset, 0, nullptr);
			vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, skinnedplayout, 2, 1,
			                        &rdjointmatrixssbo.dset, 0, nullptr);

			mgltf->drawinstanced(objs, skinnedplayout, skinnedpline, skinnedplineuint, numinstancess, stride);
		} else {
			vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, staticplayout, 1, 1,
			                        &rdperspviewmatrixubo[0].dset, 0, nullptr);
			vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, staticplayout, 2, 1,
			                        &rdjointmatrixssbo.dset, 0, nullptr);

			mgltf->drawinstancedstatic(objs, skinnedplayout, staticpline, numinstancess, stride);
		}
	}
}
