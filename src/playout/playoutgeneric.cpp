#include "playoutgeneric.hpp"
#include "modelsettings.hpp"
#include "playout.hpp"
#include "pline.hpp"
#include "ssbo.hpp"
#include "ubo.hpp"

bool playoutgeneric::setup(vkobjs &objs, std::string fname, int count, std::string vfile, std::string ffile) {
	if (!createubo(objs))
		return false;
	if (!loadmodel(objs, fname))
		return false;
	if (!createinstances(objs, count, false))
		return false;
	if (!createssbomat(objs))
		return false;
	if (!createssbodq(objs))
		return false;
	if (!createssbodecay(objs))
		return false;
	if (!createssbouint(objs))
		return false;

	if (!createplayout(objs))
		return false;
	if (!createpline(objs, vfile, ffile))
		return false;

	ready = true;

	return true;
}

bool playoutgeneric::loadmodel(vkobjs &objs, std::string fname) {
	mgltf = std::make_shared<genericmodel>();
	if (!mgltf->loadmodel(objs, fname))
		return false;
	return true;
}

bool playoutgeneric::createinstances(vkobjs &objs, int count, bool rand) {
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
bool playoutgeneric::createdecayinstances(vkobjs &objs) {
	decayinstances.clear();
	for (int i{0}; i < 20; i++) {
		float x = minstances[0]->getinstancesettings().msworldpos.x;
		float z = minstances[0]->getinstancesettings().msworldpos.z;
		decayinstances.emplace_back(std::make_shared<genericinstance>(mgltf, glm::vec3{x, 0.0f, z}, false));
	}
	return true;
}
bool playoutgeneric::createubo(vkobjs &objs) {
	if (!ubo::init(objs, rdperspviewmatrixubo))
		return false;
	desclayouts.push_back(rdperspviewmatrixubo[0].dlayout);
	return true;
}

bool playoutgeneric::createssbomat(vkobjs &objs) {
	size_t size = numinstancess * SDL_clamp(minstances[0]->getjointmatrixsize(), 1, minstances[0]->getjointmatrixsize()) *
	              sizeof(glm::mat4);
	if (!ssbo::init(objs, rdjointmatrixssbo, size))
		return false;
	desclayouts.push_back(rdjointmatrixssbo.dlayout);
	return true;
}
bool playoutgeneric::createssbouint(vkobjs &objs) {
	size_t size = numinstancess * SDL_clamp(minstances[0]->getjointmatrixsize(), 1, minstances[0]->getjointmatrixsize()) *
	              sizeof(glm::mat4);
	if (!ssbo::init(objs, uintssbo, size))
		return false;
	desclayouts.push_back(uintssbo.dlayout);
	return true;
}

bool playoutgeneric::createssbodq(vkobjs &objs) {
	size_t size = numinstancess * SDL_clamp(minstances[0]->getjointmatrixsize(), 1, minstances[0]->getjointmatrixsize()) *
	              sizeof(glm::mat2x4);
	if (!ssbo::init(objs, rdjointdualquatssbo, size))
		return false;
	desclayouts.push_back(rdjointdualquatssbo.dlayout);
	return true;
}
bool playoutgeneric::createssbodecay(vkobjs &objs) {
	size_t size = numinstancess * SDL_clamp(minstances[0]->getjointmatrixsize(), 1, minstances[0]->getjointmatrixsize()) *
	              sizeof(glm::mat4);
	if (!ssbo::init(objs, rdjointdecay, size))
		return false;
	desclayouts.push_back(rdjointdecay.dlayout);
	return true;
}

bool playoutgeneric::createplayout(vkobjs &objs) {
	texdatapls texdatapls0 = mgltf->gettexdatapls();
	desclayouts.insert(desclayouts.begin(), texdatapls0.dlayout);
	if (!playout::init(objs, rdgltfpipelinelayout, desclayouts, sizeof(vkpushconstants)))
		return false;
	return true;
}

bool playoutgeneric::createpline(vkobjs &objs, std::string vfile, std::string ffile) {
	if (!pline::init(objs, rdgltfpipelinelayout, rdgltfgpupipeline, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 5, 31,
	                 std::vector<std::string> {vfile, ffile}))
		return false;
	if (!pline::init(objs, rdgltfpipelinelayout, rdgltfgpupipelineuint, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 5, 31,
	                 std::vector<std::string> {"shaders/playeruint.vert.spv", "shaders/playeruint.frag.spv"}, true))
		return false;
	return true;
}

void playoutgeneric::updateanims() {
	for (auto &i : minstances) {
		i->updateanimation();
		i->solveik();
	}
}

void playoutgeneric::uploadvboebo(vkobjs &objs, VkCommandBuffer &cbuffer) {
	if (uploadreq) {
		mgltf->uploadvboebo(objs, cbuffer);
		uploadreq = false;
	}
}

void playoutgeneric::uploadubossbo(vkobjs &objs, std::vector<glm::mat4> &cammats) {
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1,
	                        &rdperspviewmatrixubo[0].dset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1,
	                        &rdjointmatrixssbo.dset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 3, 1,
	                        &rdjointdualquatssbo.dset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 4, 1,
	                        &rdjointdecay.dset, 0, nullptr);
	vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 5, 1,
	                        &uintssbo.dset, 0, nullptr);

	ubo::upload(objs, rdperspviewmatrixubo, cammats, 0);
	ssbo::upload(objs, rdjointmatrixssbo, jointmats);
	ssbo::upload(objs, rdjointdualquatssbo, jointdqs);
	ssbo::upload(objs, rdjointdecay, decaymat);
	ssbo::upload(objs, uintssbo, jointmats);
}

std::shared_ptr<genericinstance> playoutgeneric::getinst(int i) {
	return minstances[i];
}

std::shared_ptr<genericinstance> playoutgeneric::getdecayinst(int i) {
	return decayinstances[i];
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

void playoutgeneric::freezedecay() {
	decaymat.clear();
	minstances[0]->checkforupdates();
	minstances[0]->updateanimation();
	std::vector<glm::mat4> mats = minstances[0]->getjointmats();
	decaymat.insert(decaymat.end(), mats.begin(), mats.end());
}

void playoutgeneric::cleanuplines(vkobjs &objs) {
	pline::cleanup(objs, rdgltfgpudqpipeline);
	pline::cleanup(objs, rdgltfgpupipeline);
	pline::cleanup(objs, decaypline);
	pline::cleanup(objs, rdgltfgpudqpipelineuint);
	pline::cleanup(objs, rdgltfgpupipelineuint);
	pline::cleanup(objs, decayplineuint);
	playout::cleanup(objs, rdgltfpipelinelayout);
}

void playoutgeneric::cleanupbuffers(vkobjs &objs) {
	ubo::cleanup(objs, rdperspviewmatrixubo);
	ssbo::cleanup(objs, rdjointdualquatssbo);
	ssbo::cleanup(objs, rdjointmatrixssbo);
	ssbo::cleanup(objs, rdjointdecay);
	ssbo::cleanup(objs, uintssbo);
}

void playoutgeneric::cleanupmodels(vkobjs &objs) {
	mgltf->cleanup(objs);
	mgltf.reset();
}

void playoutgeneric::draw(vkobjs &objs) {
	if (minstances[0]->getinstancesettings().msdrawmodel) {
		stride = minstances.at(0)->getjointmatrixsize();
		stridedq = minstances.at(0)->getjointdualquatssize();

		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 1, 1,
		                        &rdperspviewmatrixubo[0].dset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 2, 1,
		                        &rdjointmatrixssbo.dset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 3, 1,
		                        &rdjointdualquatssbo.dset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 4, 1,
		                        &rdjointdecay.dset, 0, nullptr);
		vkCmdBindDescriptorSets(objs.rdcommandbuffer[0], VK_PIPELINE_BIND_POINT_GRAPHICS, rdgltfpipelinelayout, 5, 1,
		                        &uintssbo.dset, 0, nullptr);

		mgltf->drawinstanced(objs, rdgltfpipelinelayout, rdgltfgpupipeline, rdgltfgpupipelineuint, numinstancess, stride);
	}
}
