#include "playoutgeneric.hpp"
#include "buffer/ssbo.hpp"
#include "modelsettings.hpp"
#include "playout.hpp"
#include "pline.hpp"
#include "ubo.hpp"

bool playoutgeneric::setup(rvk &objs, std::string fname, size_t count, std::string vfile, std::string ffile) {
	if (!createubo(objs))
		return false;
	if (!loadmodel(objs, fname))
		return false;
	if (!createinstances(objs, count, false))
		return false;
	if (mgltf->skinned) {
		if (!createssbomat(objs))
			return false;
		if (!createplayout(objs))
			return false;
		if (!createpline(objs, vfile, ffile))
			return false;
	} else {
		if (!createplinestatic(objs, vfile, ffile))
			return false;
	}
	ready = true;

	return true;
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
	if (!ubo::init(objs, rdperspviewmatrixubo))
		return false;
	desclayouts.push_back(rdperspviewmatrixubo[0].dlayout);
	return true;
}

bool playoutgeneric::createssbomat(rvk &objs) {
	size_t size = numinstancess * SDL_clamp(minstances[0]->getjointmatrixsize(), 1, minstances[0]->getjointmatrixsize()) *
	              sizeof(glm::mat4);
	if (!ssbo::init(objs, rdjointmatrixssbo, size))
		return false;
	desclayouts.push_back(rdjointmatrixssbo.dlayout);
	return true;
}
bool playoutgeneric::createplayout(rvk &objs) {
	texdataset texdatapls0 = mgltf->gettexdatapls();
	desclayouts.insert(desclayouts.begin(), texdatapls0.dlayout);
	if (!playout::init(objs, rdgltfpipelinelayout, desclayouts, sizeof(vkpushconstants)))
		return false;
	return true;
}

bool playoutgeneric::createpline(rvk &objs, std::string vfile, std::string ffile) {
	if (!pline::init(objs, rdgltfpipelinelayout, rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 5, 31,
	                 std::vector<std::string> {vfile, ffile}))
		return false;
	if (!pline::init(objs, rdgltfpipelinelayout, rdgltfgpupipelineuint, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 5, 31,
	                 std::vector<std::string> {vfile, ffile}, true))
		return false;
	return true;
}

void playoutgeneric::updateanims() {
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
	vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1,
	                        &rdperspviewmatrixubo[0].dset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1,
	                        &rdjointmatrixssbo.dset, 0, nullptr);
	ubo::upload(objs, rdperspviewmatrixubo, cammats);
	ssbo::upload(objs, rdjointmatrixssbo, jointmats);
}

std::shared_ptr<genericinstance> playoutgeneric::getinst(int i) {
	return minstances[i];
}

void playoutgeneric::updatemats() {

	totaltricount = 0;
	jointmats.clear();
	jointdqs.clear();

	numdqs = 0;
	nummats = 0;

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
}

void playoutgeneric::cleanuplines(rvk &objs) {
	pline::cleanup(objs, rdgltfgpupipeline);
	pline::cleanup(objs, rdgltfgpupipelineuint);
	playout::cleanup(objs, rdgltfpipelinelayout);
}

void playoutgeneric::cleanupbuffers(rvk &objs) {
	ubo::cleanup(objs, rdperspviewmatrixubo);
	ssbo::cleanup(objs, rdjointmatrixssbo);
}

void playoutgeneric::cleanupmodels(rvk &objs) {
	mgltf->cleanup(objs);
	mgltf.reset();
}

void playoutgeneric::draw(rvk &objs) {
	if (minstances[0]->getinstancesettings().msdrawmodel) {
		stride = minstances.at(0)->getjointmatrixsize();

		vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1,
		                        &rdperspviewmatrixubo[0].dset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.cbuffers_graphics.at(0), VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1,
		                        &rdjointmatrixssbo.dset, 0, nullptr);

		mgltf->drawinstanced(objs, rdgltfpipelinelayout, rdgltfgpupipeline, rdgltfgpupipelineuint, numinstancess, stride);
	}
}
